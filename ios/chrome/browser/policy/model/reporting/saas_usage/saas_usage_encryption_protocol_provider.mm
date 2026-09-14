// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_encryption_protocol_provider.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#import <string_view>
#import <utility>

#import "base/functional/bind.h"
#import "base/memory/scoped_refptr.h"
#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"
#import "net/base/apple/url_conversions.h"
#import "net/ssl/ssl_connection_status_flags.h"

namespace {

net::SSLVersion SSLVersionFromTLSProtocolVersion(
    tls_protocol_version_t version) {
  // Clang complains about `tls_protocol_version_TLSv10`, etc. being deprecated.
  // Ignore the warning because we're not actually *using* them.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  switch (version) {
    case tls_protocol_version_TLSv13:
      return net::SSL_CONNECTION_VERSION_TLS1_3;
    case tls_protocol_version_TLSv12:
      return net::SSL_CONNECTION_VERSION_TLS1_2;
    case tls_protocol_version_TLSv11:
      return net::SSL_CONNECTION_VERSION_TLS1_1;
    case tls_protocol_version_TLSv10:
      return net::SSL_CONNECTION_VERSION_TLS1;
    // Standard HTTP requests can't use DTLS, which is for UDP.
    case tls_protocol_version_DTLSv12:
    case tls_protocol_version_DTLSv10:
      return net::SSL_CONNECTION_VERSION_UNKNOWN;
      // no `default`, so we add case labels as needed on SDK upgrade.
  }
#pragma clang diagnostic pop
}

using EncryptionProtocolCallback = enterprise_reporting::
    SaasUsageEncryptionProtocolProvider::EncryptionProtocolCallback;

}  // namespace

// Objective-C delegate for the HEAD request.
@interface SaasUsageProbeSessionDelegate : NSObject <NSURLSessionTaskDelegate>
- (instancetype)initWithCallback:(EncryptionProtocolCallback)callback;
@end

@implementation SaasUsageProbeSessionDelegate {
  EncryptionProtocolCallback _callback;
  scoped_refptr<base::SequencedTaskRunner> _callbackRunner;
  std::string_view _protocol;
}

- (instancetype)initWithCallback:(EncryptionProtocolCallback)callback {
  if ((self = [super init])) {
    _callback = std::move(callback);
    _callbackRunner = base::SequencedTaskRunner::GetCurrentDefault();
    _protocol = enterprise_reporting::GetEncryptionProtocolString(
        net::SSL_CONNECTION_VERSION_UNKNOWN);
  }
  return self;
}

#pragma mark - NSURLSessionTaskDelegate

- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    didFinishCollectingMetrics:(NSURLSessionTaskMetrics*)metrics {
  // Use the last transaction protocol, consistent with desktop behavior.
  for (NSURLSessionTaskTransactionMetrics* transaction in metrics
           .transactionMetrics) {
    NSNumber* protocolVersion = transaction.negotiatedTLSProtocolVersion;
    if (protocolVersion) {
      auto version = static_cast<tls_protocol_version_t>(
          [protocolVersion unsignedShortValue]);
      _protocol = enterprise_reporting::GetEncryptionProtocolString(
          SSLVersionFromTLSProtocolVersion(version));
    }
  }
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  if (!_callback.is_null()) {
    _callbackRunner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(_callback), _protocol));
  }
  [session finishTasksAndInvalidate];
}

@end

namespace enterprise_reporting {

namespace {

class ProberImpl : public SaasUsageEncryptionProtocolProvider::Prober {
 public:
  ProberImpl() = default;
  ~ProberImpl() override = default;

  void ProbeUrl(const GURL& url, EncryptionProtocolCallback callback) override {
    NSURL* ns_url = net::NSURLWithGURL(url);
    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:ns_url];
    [request setHTTPMethod:@"HEAD"];
    [request setTimeoutInterval:10.0];

    NSURLSessionConfiguration* config =
        [NSURLSessionConfiguration ephemeralSessionConfiguration];
    config.timeoutIntervalForRequest = 10.0;
    config.timeoutIntervalForResource = 10.0;

    SaasUsageProbeSessionDelegate* delegate =
        [[SaasUsageProbeSessionDelegate alloc]
            initWithCallback:std::move(callback)];

    NSURLSession* session =
        [NSURLSession sessionWithConfiguration:config
                                      delegate:delegate
                                 delegateQueue:[NSOperationQueue mainQueue]];
    NSURLSessionDataTask* task = [session dataTaskWithRequest:request];
    [task resume];
  }
};

}  // namespace

// static
SaasUsageEncryptionProtocolProvider&
SaasUsageEncryptionProtocolProvider::GetInstance() {
  static base::NoDestructor<SaasUsageEncryptionProtocolProvider> instance;
  return *instance;
}

SaasUsageEncryptionProtocolProvider::SaasUsageEncryptionProtocolProvider()
    : prober_(std::make_unique<ProberImpl>()) {}

SaasUsageEncryptionProtocolProvider::~SaasUsageEncryptionProtocolProvider() =
    default;

void SaasUsageEncryptionProtocolProvider::SetProberForTesting(
    std::unique_ptr<Prober> prober) {
  prober_ = prober ? std::move(prober) : std::make_unique<ProberImpl>();
  cache_.clear();
  pending_callbacks_.clear();
}

void SaasUsageEncryptionProtocolProvider::GetEncryptionProtocol(
    const GURL& url,
    EncryptionProtocolCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(url.is_valid());
  CHECK_NE(url.host(), "");

  if (!url.SchemeIsCryptographic()) {
    std::move(callback).Run(GetEncryptionProtocolString(std::nullopt));
    return;
  }

  std::string domain(url.host());
  if (auto it = cache_.find(domain); it != cache_.end()) {
    std::move(callback).Run(it->second);
    return;
  }

  if (auto pending_it = pending_callbacks_.find(domain);
      pending_it != pending_callbacks_.end()) {
    pending_it->second.push_back(std::move(callback));
    return;
  }

  pending_callbacks_[domain].push_back(std::move(callback));
  prober_->ProbeUrl(
      url, base::BindOnce(&SaasUsageEncryptionProtocolProvider::OnProbeComplete,
                          weak_ptr_factory_.GetWeakPtr(), std::move(domain)));
}

void SaasUsageEncryptionProtocolProvider::OnProbeComplete(
    std::string domain,
    std::string_view protocol) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& cached_protocol = (cache_[domain] = std::move(protocol));
  auto pending_it = pending_callbacks_.find(domain);
  if (pending_it == pending_callbacks_.end()) {
    return;
  }
  auto callbacks = std::move(pending_it->second);
  pending_callbacks_.erase(pending_it);
  for (auto& cb : callbacks) {
    std::move(cb).Run(cached_protocol);
  }
}

}  // namespace enterprise_reporting
