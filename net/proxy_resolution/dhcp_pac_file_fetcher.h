// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_DHCP_PAC_FILE_FETCHER_H_
#define NET_PROXY_RESOLUTION_DHCP_PAC_FILE_FETCHER_H_

#include <string>

#include "base/compiler_specific.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/pac_file_fetcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace net {

class NetLogWithSource;

// Interface for classes that can fetch a PAC file as configured via DHCP.
//
// The Fetch method on this interface tries to retrieve the most appropriate
// PAC script configured via DHCP.
//
// Normally there are zero or one DHCP scripts configured, but in the
// presence of multiple adapters with DHCP enabled, the fetcher resolves
// which PAC script to use if one or more are available.
class NET_EXPORT_PRIVATE DhcpPacFileFetcher {
 public:
  DhcpPacFileFetcher(const DhcpPacFileFetcher&) = delete;
  DhcpPacFileFetcher& operator=(const DhcpPacFileFetcher&) = delete;

  // Destruction should cancel any outstanding requests.
  virtual ~DhcpPacFileFetcher();

  // Attempts to retrieve the most appropriate PAC script configured via DHCP,
  // and invokes |callback| on completion.
  //
  // Returns OK on success, otherwise the error code. If the return code is
  // ERR_IO_PENDING, then the request completes asynchronously, and |callback|
  // will be invoked later with the final error code.
  //
  // After synchronous or asynchronous completion with a result code of OK,
  // |*utf16_text| is filled with the response. On failure, the result text is
  // an empty string, and the result code is a network error. Some special
  // network errors that may occur are:
  //
  //    ERR_PAC_NOT_IN_DHCP   -- no script configured in DHCP.
  //
  //    The following all indicate there was one or more script configured
  //    in DHCP but all failed to download, and the error for the most
  //    preferred adapter that had a script configured was what the error
  //    code says:
  //
  //      ERR_TIMED_OUT         -- fetch took too long to complete.
  //      ERR_FILE_TOO_BIG      -- response body was too large.
  //      ERR_HTTP_RESPONSE_CODE_FAILURE -- script downloaded but returned a
  //                                        non-200 HTTP response.
  //      ERR_NOT_IMPLEMENTED   -- script required authentication.
  //
  // If the request is cancelled (either using the "Cancel()" method or by
  // deleting |this|), then no callback is invoked.
  //
  // Only one fetch is allowed to be outstanding at a time.
  virtual int Fetch(std::u16string* utf16_text,
                    CompletionOnceCallback callback,
                    const NetLogWithSource& net_log,
                    const NetworkTrafficAnnotationTag traffic_annotation) = 0;

  // Aborts the in-progress fetch (if any).
  virtual void Cancel() = 0;

  // Cancels the in-progress fetch (if any), without invoking its callback.
  // Future requests will fail immediately. Must be called before the
  // URLRequestContext the fetcher was created with is torn down.
  virtual void OnShutdown() = 0;

  // After successful completion of |Fetch()|, this will return the URL
  // retrieved from DHCP.  It is reset if/when |Fetch()| is called again.
  virtual const GURL& GetPacURL() const = 0;

  // Intended for unit tests only, so they can test that factories return
  // the right types under given circumstances.
  virtual std::string GetFetcherName() const;

 protected:
  DhcpPacFileFetcher();
};

// A do-nothing retriever, always returns synchronously with
// ERR_NOT_IMPLEMENTED result and empty text.
class NET_EXPORT_PRIVATE DoNothingDhcpPacFileFetcher
    : public DhcpPacFileFetcher {
 public:
  DoNothingDhcpPacFileFetcher();

  DoNothingDhcpPacFileFetcher(const DoNothingDhcpPacFileFetcher&) = delete;
  DoNothingDhcpPacFileFetcher& operator=(const DoNothingDhcpPacFileFetcher&) =
      delete;

  ~DoNothingDhcpPacFileFetcher() override;

  int Fetch(std::u16string* utf16_text,
            CompletionOnceCallback callback,
            const NetLogWithSource& net_log,
            const NetworkTrafficAnnotationTag traffic_annotation) override;
  void Cancel() override;
  void OnShutdown() override;
  const GURL& GetPacURL() const override;
  std::string GetFetcherName() const override;

 private:
  GURL gurl_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_DHCP_PAC_FILE_FETCHER_H_
