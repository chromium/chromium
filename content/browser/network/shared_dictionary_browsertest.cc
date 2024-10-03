// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>
#include <tuple>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "net/base/hash_value.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/extras/shared_dictionary/shared_dictionary_usage_info.h"
#include "net/shared_dictionary/shared_dictionary_constants.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

using ::testing::UnorderedElementsAreArray;

namespace content {

namespace {

// The Structured Field sf-binary hash of sha256 of dictionary.
// (content/test/data/shared_dictionary/test.dict and test_dict.html).
constexpr std::string_view kExpectedDictionaryHashBase64 =
    ":U5abz16WDg7b8KS93msLPpOB4Vbef1uRzoORYkJw9BY=:";
constexpr net::SHA256HashValue kExpectedDictionaryHashValue = {
    {0x53, 0x96, 0x9b, 0xcf, 0x5e, 0x96, 0x0e, 0x0e, 0xdb, 0xf0, 0xa4,
     0xbd, 0xde, 0x6b, 0x0b, 0x3e, 0x93, 0x81, 0xe1, 0x56, 0xde, 0x7f,
     0x5b, 0x91, 0xce, 0x83, 0x91, 0x62, 0x42, 0x70, 0xf4, 0x16}};

constexpr std::string_view kUncompressedDataString =
    "test(\"This is uncompressed.\");";
constexpr std::string_view kErrorInvalidHashString =
    "test(\"Invalid dictionary hash.\");";
constexpr std::string_view kErrorNoSharedDictionaryAcceptEncodingString =
    "test(\"dcb or dcz is not set in accept-encoding header.\");";

constexpr std::string_view kCompressedDataOriginalString =
    "test(\"This is compressed test data using a test dictionary\");";

// kBrotliCompressedData is generated using the following commands:
// $ echo "This is a test dictionary." > /tmp/dict
// $ echo -n "test(\"This is compressed test data using a test dictionary\");" \
//     > /tmp/data
// $ echo -en '\xffDCB' > /tmp/out.dcb
// $ openssl dgst -sha256 -binary /tmp/dict >> /tmp/out.dcb
// $ brotli --stdout -D /tmp/dict /tmp/data >> /tmp/out.dcb
// $ xxd -i /tmp/out.dcb
constexpr uint8_t kBrotliCompressedData[] = {
    0xff, 0x44, 0x43, 0x42, 0x53, 0x96, 0x9b, 0xcf, 0x5e, 0x96, 0x0e, 0x0e,
    0xdb, 0xf0, 0xa4, 0xbd, 0xde, 0x6b, 0x0b, 0x3e, 0x93, 0x81, 0xe1, 0x56,
    0xde, 0x7f, 0x5b, 0x91, 0xce, 0x83, 0x91, 0x62, 0x42, 0x70, 0xf4, 0x16,
    0xa1, 0xe0, 0x01, 0x00, 0x64, 0x9c, 0xa4, 0xaa, 0xd7, 0x47, 0xe0, 0x26,
    0x4b, 0x95, 0x91, 0xb4, 0x46, 0x36, 0x09, 0xc9, 0xc7, 0x0e, 0x38, 0xe4,
    0x44, 0xe8, 0x72, 0x0d, 0x3c, 0x6e, 0xab, 0x35, 0x9b, 0x0f, 0x4b, 0xd1,
    0x67, 0x0c, 0xec, 0x7f, 0x9d, 0x1e, 0x99, 0x10, 0xf5, 0x1e, 0x57, 0x2f};
const std::string kBrotliCompressedDataString =
    std::string(reinterpret_cast<const char*>(kBrotliCompressedData),
                sizeof(kBrotliCompressedData));

// kZstdCompressedData is generated using the following commands:
// $ echo "This is a test dictionary." > /tmp/dict
// $ echo -n "test(\"This is compressed test data using a test dictionary\");" \
//     > /tmp/data
// $ echo -en '\x5e\x2a\x4d\x18\x20\x00\x00\x00' > /tmp/out.dcz
// $ openssl dgst -sha256 -binary /tmp/dict >> /tmp/out.dcz
// $ zstd -D /tmp/dict -f -o /tmp/tmp.zstd /tmp/data
// $ cat /tmp/tmp.zstd >> /tmp/out.dcz
// $ xxd -i /tmp/out.dcz
constexpr uint8_t kZstdCompressedData[] = {
    0x5e, 0x2a, 0x4d, 0x18, 0x20, 0x00, 0x00, 0x00, 0x53, 0x96, 0x9b, 0xcf,
    0x5e, 0x96, 0x0e, 0x0e, 0xdb, 0xf0, 0xa4, 0xbd, 0xde, 0x6b, 0x0b, 0x3e,
    0x93, 0x81, 0xe1, 0x56, 0xde, 0x7f, 0x5b, 0x91, 0xce, 0x83, 0x91, 0x62,
    0x42, 0x70, 0xf4, 0x16, 0x28, 0xb5, 0x2f, 0xfd, 0x24, 0x3d, 0x35, 0x01,
    0x00, 0xe0, 0x74, 0x65, 0x73, 0x74, 0x28, 0x22, 0x63, 0x6f, 0x6d, 0x70,
    0x72, 0x65, 0x73, 0x73, 0x65, 0x64, 0x61, 0x74, 0x61, 0x20, 0x75, 0x73,
    0x69, 0x6e, 0x67, 0x22, 0x29, 0x3b, 0x03, 0x10, 0x05, 0xdf, 0x9f, 0x96,
    0x11, 0x21, 0x8a, 0x48, 0x20, 0xef, 0xeb};
const std::string kZstdCompressedDataString =
    std::string(reinterpret_cast<const char*>(kZstdCompressedData),
                sizeof(kZstdCompressedData));

constexpr std::string_view kUncompressedDataResultString =
    "This is uncompressed.";
constexpr std::string_view kCompressedDataResultString =
    "This is compressed test data using a test dictionary";

constexpr std::string_view kHttpAuthPath = "/shared_dictionary/path/http_auth";
const std::string kTestPath = "/shared_dictionary/path/test";

class SharedDictionaryAccessObserver : public WebContentsObserver {
 public:
  SharedDictionaryAccessObserver(WebContents* web_contents,
                                 base::RepeatingClosure on_accessed_callback)
      : WebContentsObserver(web_contents),
        on_accessed_callback_(std::move(on_accessed_callback)) {}

  const network::mojom::SharedDictionaryAccessDetailsPtr& details() const {
    return details_;
  }

 private:
  // WebContentsObserver overrides:
  void OnSharedDictionaryAccessed(
      NavigationHandle* navigation,
      const network::mojom::SharedDictionaryAccessDetails& details) override {
    details_ = details.Clone();
    on_accessed_callback_.Run();
  }
  void OnSharedDictionaryAccessed(
      RenderFrameHost* rfh,
      const network::mojom::SharedDictionaryAccessDetails& details) override {
    details_ = details.Clone();
    on_accessed_callback_.Run();
  }

  base::RepeatingClosure on_accessed_callback_;
  network::mojom::SharedDictionaryAccessDetailsPtr details_;
};

bool WaitForHistogram(const std::string& histogram_name,
                      std::optional<base::TimeDelta> timeout = std::nullopt) {
  // Need the polling of histogram because ScopedHistogramSampleObserver doesn't
  // support cross process metrics.
  base::Time start_time = base::Time::Now();
  while (!base::StatisticsRecorder::FindHistogram(histogram_name)) {
    content::FetchHistogramsFromChildProcesses();
    base::PlatformThread::Sleep(base::Milliseconds(5));
    if (timeout && base::Time::Now() > start_time + *timeout) {
      return false;
    }
  }
  return true;
}

enum class FeatureState {
  kDisabled,
  kBackendOnly,
  kFullyEnabled,
  kFullyEnabledWithZstd
};

enum class BrowserType { kNormal, kOffTheRecord };
std::string ToString(BrowserType browser_type) {
  switch (browser_type) {
    case BrowserType::kNormal:
      return "Normal";
    case BrowserType::kOffTheRecord:
      return "OffTheRecord";
  }
}

enum class FetchType {
  kLinkRelCompressionDictionary,
  kLinkRelCompressionDictionaryDocumentHeader,
  kLinkRelCompressionDictionarySubresourceHeader,
  kFetchApi,
  kFetchApiWithSameOriginMode,
  kFetchApiWithNoCorsMode,
  kFetchApiFromDedicatedWorker,
  kFetchApiFromSharedWorker,
  kFetchApiFromServiceWorker,
  kIframeNavigation,
};

std::string LinkRelCompressionDictionaryScript(const GURL& dictionary_url) {
  return JsReplace(R"(
              (()=>{
                const link = document.createElement('link');
                link.rel = 'compression-dictionary';
                link.href = $1;
                document.body.appendChild(link);
              })();
            )",
                   dictionary_url);
}

std::string LinkRelCompressionDictionaryDocumentHeaderScript(
    const GURL& dictionary_url) {
  return JsReplace(R"(
              (()=>{
                const iframe = document.createElement('iframe');
                iframe.src = new URL('with_dict_header.html', $1);
                document.body.appendChild(iframe);
              })();
            )",
                   dictionary_url);
}

std::string LinkRelCompressionDictionarySubresourceHeaderScript(
    const GURL& dictionary_url) {
  return JsReplace(R"(
              (()=>{
                fetch(new URL('with_dict_header.html', $1));
              })();
            )",
                   dictionary_url);
}

std::string FetchDictionaryScript(const GURL& dictionary_url) {
  return JsReplace(R"(
          (async () => {
            try {
              await fetch($1);
            } catch (e) {
            }
          })();
        )",
                   dictionary_url);
}

std::string FetchDictionaryWithSameOriginModeScript(
    const GURL& dictionary_url) {
  return JsReplace(R"(
          (async () => {
            try {
              await fetch($1, {mode: 'same-origin'});
            } catch (e) {
            }
          })();
        )",
                   dictionary_url);
}

std::string FetchDictionaryWithNoCorsModeScript(const GURL& dictionary_url) {
  return JsReplace(R"(
          (async () => {
            try {
              await fetch($1, {mode: 'no-cors'});
            } catch (e) {
            }
          })();
        )",
                   dictionary_url);
}

std::string StartTestDedicatedWorkerScript(const GURL& dictionary_url) {
  return JsReplace(R"(
          (async () => {
            const script = '/shared_dictionary/fetch_dictionary.js';
            const worker = new Worker(script);
            worker.postMessage($1);
          })();
        )",
                   dictionary_url);
}

std::string StartTestSharedWorkerScript(const GURL& dictionary_url) {
  return JsReplace(R"(
          (async () => {
            const script =
                new URL(location).searchParams.has('otworker') ?
                  '/shared_dictionary/fetch_dictionary.js?ot=enabled' :
                  '/shared_dictionary/fetch_dictionary.js';
            const worker = new SharedWorker(script);
            worker.port.start();
            worker.port.postMessage($1);
          })();
        )",
                   dictionary_url);
}

std::string RegisterTestServiceWorkerScript(const GURL& dictionary_url) {
  return JsReplace(R"(
          (async () => {
            const script =
                new URL(location).searchParams.has('otworker') ?
                  '/shared_dictionary/fetch_dictionary.js?ot=enabled' :
                  '/shared_dictionary/fetch_dictionary.js';
            const registration = await navigator.serviceWorker.register(
                script,
                {scope: '/shared_dictionary/'});
            registration.installing.postMessage($1);
          })();
        )",
                   dictionary_url);
}

std::string FetchTargetDataScript(const GURL& dictionary_url) {
  return JsReplace(R"(
          (async () => {
            const url = new URL('./path/test' ,$1);
            const response = await fetch(url);
            return response.text();
          })();
        )",
                   dictionary_url);
}

std::string IframeLoadScript(const GURL& url) {
  return JsReplace(R"(
  (async () => {
    const iframe = document.createElement('iframe');
    iframe.src = $1;
    const promise =
        new Promise(resolve => { iframe.addEventListener('load', resolve); });
    document.body.appendChild(iframe);
    await promise;
    try {
      return iframe.contentDocument.body.innerText;
    } catch {
      return 'failed to access iframe';
    }
  })()
                  )",
                   url);
}
std::optional<std::string> GetAvailableDictionary(
    const net::test_server::HttpRequest::HeaderMap& headers) {
      auto it = headers.find("available-dictionary");
      return it == headers.end() ? std::nullopt
                                 : std::make_optional(it->second);
}

bool HasSharedDictionaryAcceptEncoding(
    const net::test_server::HttpRequest::HeaderMap& headers) {
  auto it = headers.find(net::HttpRequestHeaders::kAcceptEncoding);
  if (it == headers.end()) {
    return false;
  }
  if (base::FeatureList::IsEnabled(network::features::kSharedZstd)) {
    return it->second == "dcb, dcz" || base::EndsWith(it->second, ", dcb, dcz");
  } else {
    return it->second == "dcb" || base::EndsWith(it->second, ", dcb");
  }
}

// A dummy ContentBrowserClient for testing HTTP Auth.
class DummyAuthContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  DummyAuthContentBrowserClient() = default;
  ~DummyAuthContentBrowserClient() override = default;
  DummyAuthContentBrowserClient(const DummyAuthContentBrowserClient&) = delete;
  DummyAuthContentBrowserClient& operator=(
      const DummyAuthContentBrowserClient&) = delete;

  // ContentBrowserClient method:
  std::unique_ptr<LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const GlobalRequestID& request_id,
      bool is_request_for_primary_main_frame,
      bool is_request_for_navigation,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override {
    create_login_delegate_called_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(auth_required_callback),
                       net::AuthCredentials(u"username", u"password")));
    return std::make_unique<LoginDelegate>();
  }

  bool create_login_delegate_called() const {
    return create_login_delegate_called_;
  }

 private:
  bool create_login_delegate_called_ = false;
};

// A dummy ContentBrowserClient for allowing all certificate errors.
class CertificateErrorAllowingContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  CertificateErrorAllowingContentBrowserClient() = default;
  ~CertificateErrorAllowingContentBrowserClient() override = default;
  CertificateErrorAllowingContentBrowserClient(
      const CertificateErrorAllowingContentBrowserClient&) = delete;
  CertificateErrorAllowingContentBrowserClient& operator=(
      const CertificateErrorAllowingContentBrowserClient&) = delete;

  // ContentBrowserClient method:
  void AllowCertificateError(
      WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool is_primary_main_frame_request,
      bool strict_enforcement,
      base::OnceCallback<void(CertificateRequestResultType)> callback)
      override {
    allow_certificate_error_called_ = true;
    std::move(callback).Run(CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE);
  }

  bool allow_certificate_error_called() const {
    return allow_certificate_error_called_;
  }

 private:
  bool allow_certificate_error_called_ = false;
};

// A dummy ContentBrowserClient for setting client certificate.
class DummyClientCertStoreContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  DummyClientCertStoreContentBrowserClient() = default;
  ~DummyClientCertStoreContentBrowserClient() override = default;
  DummyClientCertStoreContentBrowserClient(
      const DummyClientCertStoreContentBrowserClient&) = delete;
  DummyClientCertStoreContentBrowserClient& operator=(
      const DummyClientCertStoreContentBrowserClient&) = delete;

  // ContentBrowserClient methods:
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      BrowserContext* browser_context) override {
    net::ClientCertIdentityList cert_identity_list;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      std::unique_ptr<net::FakeClientCertIdentity> cert_identity =
          net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
              net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
      EXPECT_TRUE(cert_identity.get());
      cert_identity_list.push_back(std::move(cert_identity));
    }
    return std::make_unique<DummyClientCertStore>(
        std::move(cert_identity_list));
  }
  base::OnceClosure SelectClientCertificate(
      BrowserContext* browser_context,
      int process_id,
      WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<ClientCertificateDelegate> delegate) override {
    select_client_certificate_called_ = true;
    CHECK_EQ(1u, client_certs.size());
    scoped_refptr<net::X509Certificate> cert(client_certs[0]->certificate());
    client_certs[0]->AcquirePrivateKey(base::BindOnce(
        [](std::unique_ptr<ClientCertificateDelegate> delegate,
           scoped_refptr<net::X509Certificate> cert,
           scoped_refptr<net::SSLPrivateKey> key) {
          delegate->ContinueWithCertificate(std::move(cert), std::move(key));
        },
        std::move(delegate), std::move(cert)));
    return base::OnceClosure();
  }

  bool select_client_certificate_called() const {
    return select_client_certificate_called_;
  }

 private:
  class DummyClientCertStore : public net::ClientCertStore {
   public:
    explicit DummyClientCertStore(net::ClientCertIdentityList list)
        : list_(std::move(list)) {}
    ~DummyClientCertStore() override = default;

    // net::ClientCertStore:
    void GetClientCerts(
        scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
        ClientCertListCallback callback) override {
      std::move(callback).Run(std::move(list_));
    }

   private:
    net::ClientCertIdentityList list_;
  };
  bool select_client_certificate_called_ = false;
};

class SharedDictionaryBrowserTestBase : public ContentBrowserTest {
 public:
  SharedDictionaryBrowserTestBase() = default;

  SharedDictionaryBrowserTestBase(const SharedDictionaryBrowserTestBase&) =
      delete;
  SharedDictionaryBrowserTestBase& operator=(
      const SharedDictionaryBrowserTestBase&) = delete;

 protected:
  int64_t GetTestDataFileSize(const std::string& name) {
    base::FilePath file_path;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path));
    int64_t file_size = 0;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::GetFileSize(
          file_path.Append(GetTestDataFilePath()).AppendASCII(name),
          &file_size));
    }
    return file_size;
  }
  std::string GetTestDataFile(const std::string& name) {
    base::FilePath file_path;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path));
    std::string contents;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::ReadFileToString(
          file_path.Append(GetTestDataFilePath()).AppendASCII(name),
          &contents));
    }
    return contents;
  }

  void RunWriteDictionaryTestImpl(Shell* shell,
                                  FetchType fetch_type,
                                  const GURL& page_url,
                                  const GURL& dictionary_url,
                                  const std::string& histogram_name,
                                  bool expect_success,
                                  bool navigate_to_page_url = true) {
    if (navigate_to_page_url) {
      EXPECT_TRUE(NavigateToURL(shell, page_url));
    }
    base::RunLoop write_loop;
    auto write_observer = std::make_unique<SharedDictionaryAccessObserver>(
        shell->web_contents(), write_loop.QuitClosure());

    base::HistogramTester histogram_tester;
    std::string script;
    switch (fetch_type) {
      case FetchType::kLinkRelCompressionDictionary:
        script = LinkRelCompressionDictionaryScript(dictionary_url);
        break;
      case FetchType::kLinkRelCompressionDictionaryDocumentHeader:
        script =
            LinkRelCompressionDictionaryDocumentHeaderScript(dictionary_url);
        break;
      case FetchType::kLinkRelCompressionDictionarySubresourceHeader:
        script =
            LinkRelCompressionDictionarySubresourceHeaderScript(dictionary_url);
        break;
      case FetchType::kFetchApi:
        script = FetchDictionaryScript(dictionary_url);
        break;
      case FetchType::kFetchApiWithSameOriginMode:
        script = FetchDictionaryWithSameOriginModeScript(dictionary_url);
        break;
      case FetchType::kFetchApiWithNoCorsMode:
        script = FetchDictionaryWithNoCorsModeScript(dictionary_url);
        break;
      case FetchType::kFetchApiFromDedicatedWorker:
        script = StartTestDedicatedWorkerScript(dictionary_url);
        break;
      case FetchType::kFetchApiFromSharedWorker:
        script = StartTestSharedWorkerScript(dictionary_url);
        break;
      case FetchType::kFetchApiFromServiceWorker:
        script = RegisterTestServiceWorkerScript(dictionary_url);
        break;
      case FetchType::kIframeNavigation:
        script = IframeLoadScript(dictionary_url);
        break;
    }
    EXPECT_TRUE(ExecJs(shell->web_contents()->GetPrimaryMainFrame(), script));
    if (expect_success) {
      write_loop.Run();
      ASSERT_TRUE(write_observer->details());
      EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kWrite,
                write_observer->details()->type);
      EXPECT_EQ(dictionary_url, write_observer->details()->url);
      EXPECT_EQ(net::SharedDictionaryIsolationKey(url::Origin::Create(page_url),
                                                  net::SchemefulSite(page_url)),
                write_observer->details()->isolation_key);
      EXPECT_FALSE(write_observer->details()->is_blocked);
    }

    if (!expect_success) {
      EXPECT_FALSE(WaitForHistogram(histogram_name, base::Milliseconds(100)));
      EXPECT_FALSE(write_observer->details());

      EXPECT_EQ(kUncompressedDataString,
                EvalJs(shell->web_contents()->GetPrimaryMainFrame(),
                       FetchTargetDataScript(dictionary_url))
                    .ExtractString());
      return;
    }
    EXPECT_TRUE(WaitForHistogram(histogram_name));
    histogram_tester.ExpectBucketCount(
        histogram_name, GetTestDataFileSize("shared_dictionary/test.dict"),
        /*expected_count=*/1);
    base::RunLoop read_loop;

    auto read_observer = std::make_unique<SharedDictionaryAccessObserver>(
        shell->web_contents(), read_loop.QuitClosure());
    EXPECT_EQ(kCompressedDataOriginalString,
              EvalJs(shell->web_contents()->GetPrimaryMainFrame(),
                     FetchTargetDataScript(dictionary_url))
                  .ExtractString());
    read_loop.Run();
    ASSERT_TRUE(read_observer->details());
    EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kRead,
              read_observer->details()->type);
    EXPECT_EQ(dictionary_url.Resolve("path/test"),
              read_observer->details()->url);
    EXPECT_EQ(net::SharedDictionaryIsolationKey(url::Origin::Create(page_url),
                                                net::SchemefulSite(page_url)),
              read_observer->details()->isolation_key);
    EXPECT_FALSE(read_observer->details()->is_blocked);
  }

  void RegisterTestRequestHandler(net::EmbeddedTestServer& server) {
    server.RegisterRequestHandler(
        base::BindRepeating(&SharedDictionaryBrowserTestBase::RequestHandler,
                            base::Unretained(this)));
  }

  std::vector<net::SharedDictionaryUsageInfo> GetSharedDictionaryUsageInfo(
      Shell* shell) {
    base::test::TestFuture<const std::vector<net::SharedDictionaryUsageInfo>&>
        result;
    shell->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetSharedDictionaryUsageInfo(result.GetCallback());
    return result.Get();
  }

  std::vector<url::Origin> GetOriginsBetween(Shell* shell,
                                             base::Time start_time,
                                             base::Time end_time) {
    base::test::TestFuture<const std::vector<url::Origin>&> result;
    shell->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetSharedDictionaryOriginsBetween(start_time, end_time,
                                            result.GetCallback());
    return result.Get();
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext();
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url, kTestPath)) {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);

    if (request.GetURL().query() == "html") {
      response->set_content_type("text/html");
    } else {
      response->set_content_type("application/javascript");
    }

    if (request.GetURL().query() != "no_acao" &&
        request.headers.find("origin") != request.headers.end()) {
      response->AddCustomHeader("Access-Control-Allow-Credentials", "true");
      response->AddCustomHeader("Access-Control-Allow-Origin",
                                request.headers.at("origin"));
    }
    std::optional<std::string> dict_hash =
        GetAvailableDictionary(request.headers);
    if (dict_hash) {
      if (*dict_hash == kExpectedDictionaryHashBase64) {
        if (HasSharedDictionaryAcceptEncoding(request.headers)) {
          if (base::FeatureList::IsEnabled(network::features::kSharedZstd)) {
            response->AddCustomHeader(
                "content-encoding",
                net::shared_dictionary::kSharedZstdContentEncodingName);
            response->set_content(kZstdCompressedDataString);
          } else {
            response->AddCustomHeader(
                "content-encoding",
                net::shared_dictionary::kSharedBrotliContentEncodingName);
            response->set_content(kBrotliCompressedDataString);
          }
        } else {
          response->set_content(kErrorNoSharedDictionaryAcceptEncodingString);
        }
      } else {
        response->set_content(kErrorInvalidHashString);
      }
    } else {
      response->set_content(kUncompressedDataString);
    }

    return response;
  }
};

// Tests end to end functionality of "compression dictionary transport" feature
// with fully enabled features.
class SharedDictionaryBrowserTest
    : public SharedDictionaryBrowserTestBase,
      public ::testing::WithParamInterface<BrowserType> {
 public:
  SharedDictionaryBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {network::features::kCompressionDictionaryTransportBackend,
         network::features::kCompressionDictionaryTransport,
         network::features::kSharedZstd},
        /*disabled_features=*/{});
  }
  SharedDictionaryBrowserTest(const SharedDictionaryBrowserTest&) = delete;
  SharedDictionaryBrowserTest& operator=(const SharedDictionaryBrowserTest&) =
      delete;

  // ContentBrowserTest implementation:
  void SetUpOnMainThread() override {
    RegisterTestRequestHandler(*embedded_test_server());
    RegisterRedirectRequestHandler(*embedded_test_server());
    RegisterClearSiteDataRequestHandler(*embedded_test_server());
    RegisterHttpAuthRequestHandler(*embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    cross_origin_server_ = std::make_unique<net::EmbeddedTestServer>();
    cross_origin_server()->AddDefaultHandlers(GetTestDataFilePath());
    RegisterTestRequestHandler(*cross_origin_server());
    RegisterRedirectRequestHandler(*cross_origin_server());
    RegisterClearSiteDataRequestHandler(*cross_origin_server());
    ASSERT_TRUE(cross_origin_server()->Start());

    host_resolver()->AddRule("*", "127.0.0.1");
  }
  void TearDownOnMainThread() override { off_the_record_shell_ = nullptr; }

  std::string FetchSameOriginRequest(const GURL& url) {
    return EvalJs(GetTargetShell()->web_contents()->GetPrimaryMainFrame(),
                  JsReplace(R"(
          (async () => {
            try {
              const res = await fetch($1, {mode: 'same-origin'});
              return await res.text();
            } catch {
              return 'failed to fetch';
            }
          })();
        )",
                            url))
        .ExtractString();
  }

  std::string LoadTestScript(const GURL& url) {
    return EvalJs(GetTargetShell()->web_contents()->GetPrimaryMainFrame(),
                  JsReplace(R"(
          (async () => {
            return await new Promise(resolve => {
              window.test = resolve;
              const script = document.createElement('script');
              script.src = $1;
              document.body.appendChild(script);
            });
          })();
        )",
                            url))
        .ExtractString();
  }
  std::string LoadTestScriptWithCrossOriginAnonymous(const GURL& url) {
    return EvalJs(GetTargetShell()->web_contents()->GetPrimaryMainFrame(),
                  JsReplace(R"(
          (async () => {
            return await new Promise(resolve => {
              window.test = resolve;
              const script = document.createElement('script');
              script.src = $1;
              script.addEventListener('error', () => {resolve('load failed');});
              script.crossOrigin = 'anonymous';
              document.body.appendChild(script);
            });
          })();
        )",
                            url))
        .ExtractString();
  }
  std::string LoadTestScriptWithCrossOriginUseCredentials(const GURL& url) {
    return EvalJs(GetTargetShell()->web_contents()->GetPrimaryMainFrame(),
                  JsReplace(R"(
          (async () => {
            return await new Promise(resolve => {
              window.test = resolve;
              const script = document.createElement('script');
              script.src = $1;
              script.addEventListener('error', () => {resolve('load failed');});
              script.crossOrigin = 'use-credentials';
              document.body.appendChild(script);
            });
          })();
        )",
                            url))
        .ExtractString();
  }
  std::string LoadTestIframe(const GURL& url) {
    return EvalJs(GetTargetShell()->web_contents()->GetPrimaryMainFrame(),
                  IframeLoadScript(url))
        .ExtractString();
  }

 protected:
  BrowserType GetBrowserType() const { return GetParam(); }
  net::EmbeddedTestServer* cross_origin_server() const {
    return cross_origin_server_.get();
  }

  Shell* GetTargetShell() {
    if (GetBrowserType() == BrowserType::kNormal) {
      return shell();
    }
    if (!off_the_record_shell_) {
      off_the_record_shell_ = CreateOffTheRecordBrowser();
    }
    return off_the_record_shell_;
  }
  network::mojom::NetworkContext* GetTargetNetworkContext() {
    return GetTargetShell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext();
  }

  void RunWriteDictionaryTest(FetchType fetch_type,
                              const GURL& page_url,
                              const GURL& dictionary_url,
                              bool expect_success = true) {
    RunWriteDictionaryTestImpl(
        GetTargetShell(), fetch_type, page_url, dictionary_url,
        GetBrowserType() == BrowserType::kNormal
            ? "Net.SharedDictionaryManagerOnDisk.DictionarySizeKB"
            : "Net.SharedDictionaryWriterInMemory.DictionarySize",
        expect_success);
  }

  GURL GetURL(std::string_view relative_url) const {
    return embedded_test_server()->GetURL(relative_url);
  }
  GURL GetURL(std::string_view hostname, std::string_view relative_url) const {
    return embedded_test_server()->GetURL(hostname, relative_url);
  }
  GURL GetCrossOriginURL(std::string_view relative_url) const {
    return cross_origin_server()->GetURL(relative_url);
  }

  bool HasPreloadedSharedDictionaryInfo() {
    bool result = false;
    base::RunLoop run_loop;
    GetTargetNetworkContext()->HasPreloadedSharedDictionaryInfoForTesting(
        base::BindLambdaForTesting([&](bool value) {
          result = value;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void SendMemoryPressureToNetworkService() {
    content::GetNetworkService()->OnMemoryPressure(
        base::MemoryPressureListener::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_CRITICAL);
    // To make sure that OnMemoryPressure has been received by the network
    // service, send a GetNetworkList IPC and wait for the result.
    base::RunLoop run_loop;
    content::GetNetworkService()->GetNetworkList(
        net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
        base::BindLambdaForTesting(
            [&](const std::optional<net::NetworkInterfaceList>&
                    interface_list) { run_loop.Quit(); }));
    run_loop.Run();
  }

 private:
  void RegisterRedirectRequestHandler(net::EmbeddedTestServer& server) {
    server.RegisterRequestHandler(base::BindRepeating(
        &SharedDictionaryBrowserTest::RedirectRequestHandler,
        base::Unretained(this)));
  }
  std::unique_ptr<net::test_server::HttpResponse> RedirectRequestHandler(
      const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url, "/redirect")) {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_MOVED_PERMANENTLY);
    const std::string location = request.GetURL().query();
    response->AddCustomHeader("Location", location);
    if (request.headers.find("origin") != request.headers.end()) {
      response->AddCustomHeader("Access-Control-Allow-Credentials", "true");
      response->AddCustomHeader("Access-Control-Allow-Origin",
                                request.headers.at("origin"));
    }
    return response;
  }

  void RegisterClearSiteDataRequestHandler(net::EmbeddedTestServer& server) {
    server.RegisterRequestHandler(base::BindRepeating(
        &SharedDictionaryBrowserTest::ClearSiteDataRequestHandler,
        base::Unretained(this)));
  }
  std::unique_ptr<net::test_server::HttpResponse> ClearSiteDataRequestHandler(
      const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url,
                          "/shared_dictionary/clear_site_data")) {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);

    // We need these Access-Control-Allow-* headers for cross origin tests.
    if (request.headers.find("origin") != request.headers.end()) {
      response->AddCustomHeader("Access-Control-Allow-Credentials", "true");
      response->AddCustomHeader("Access-Control-Allow-Origin",
                                request.headers.at("origin"));
    }

    if (request.GetURL().query() == "cache") {
      response->AddCustomHeader("Clear-Site-Data", "\"cache\"");
    } else if (request.GetURL().query() == "cookies") {
      response->AddCustomHeader("Clear-Site-Data", "\"cookies\"");
    } else if (request.GetURL().query() == "storage") {
      response->AddCustomHeader("Clear-Site-Data", "\"storage\"");
    }
    response->set_content("");
    return response;
  }
  void RegisterHttpAuthRequestHandler(net::EmbeddedTestServer& server) {
    server.RegisterRequestHandler(base::BindRepeating(
        &SharedDictionaryBrowserTest::HttpAuthRequestHandler,
        base::Unretained(this)));
  }
  std::unique_ptr<net::test_server::HttpResponse> HttpAuthRequestHandler(
      const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url, kHttpAuthPath)) {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (base::Contains(request.headers, "Authorization")) {
      response->set_code(net::HTTP_OK);
      std::optional<std::string> dict_hash =
          GetAvailableDictionary(request.headers);
      if (dict_hash) {
        if (*dict_hash == kExpectedDictionaryHashBase64) {
          if (HasSharedDictionaryAcceptEncoding(request.headers)) {
            response->AddCustomHeader("content-encoding", "dcb");
            response->set_content(kBrotliCompressedDataString);
          } else {
            response->set_content(kErrorNoSharedDictionaryAcceptEncodingString);
          }
        } else {
          response->set_content(kErrorInvalidHashString);
        }
      } else {
        response->set_content(kUncompressedDataString);
      }
    } else {
      response->set_code(net::HTTP_UNAUTHORIZED);
      response->AddCustomHeader("WWW-Authenticate",
                                "Basic realm=\"test realm\"");
    }
    return response;
  }

  raw_ptr<Shell> off_the_record_shell_ = nullptr;
  std::unique_ptr<net::EmbeddedTestServer> cross_origin_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharedDictionaryBrowserTest,
                         testing::Values(BrowserType::kNormal,
                                         BrowserType::kOffTheRecord),
                         [](const testing::TestParamInfo<BrowserType>& info) {
                           return ToString(info.param);
                         });

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       LinkRelCompressionDictionarySecureContext) {
  // http://127.0.0.1:PORT/ is secure context, so the dictionary should be
  // written.
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       FetchDictionarySecureContext) {
  // http://127.0.0.1:PORT/ is secure context, so the dictionary should be
  // written.
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       LinkRelCompressionDictionaryDocumentHeader) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionaryDocumentHeader,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       LinkRelCompressionDictionarySubresourceHeader) {
  RunWriteDictionaryTest(
      FetchType::kLinkRelCompressionDictionarySubresourceHeader,
      GetURL("/shared_dictionary/blank.html"),
      GetURL("/shared_dictionary/test.dict"));
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       FetchDictionaryWithSameOriginMode) {
  RunWriteDictionaryTest(FetchType::kFetchApiWithSameOriginMode,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       FetchDictionaryWithNoCorsMode) {
  RunWriteDictionaryTest(FetchType::kFetchApiWithNoCorsMode,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       LinkRelCompressionDictionaryInsecureContext) {
  // http://www.test/ is insecure context, so the dictionary should not be
  // written.
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("www.test", "/shared_dictionary/blank.html"),
                         GetURL("www.test", "/shared_dictionary/test.dict"),
                         /*expect_success=*/false);
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       FetchDictionaryInsecureContext) {
  // http://www.test/ is insecure context, so the dictionary should not be
  // written.
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("www.test", "/shared_dictionary/blank.html"),
                         GetURL("www.test", "/shared_dictionary/test.dict"),
                         /*expect_success=*/false);
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       FetchDictionaryFromDedicatedWorker) {
  RunWriteDictionaryTest(FetchType::kFetchApiFromDedicatedWorker,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
}

#if !BUILDFLAG(IS_ANDROID)
// Shared workers are not supported on Android.
IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       FetchDictionaryFromSharedWorker) {
  RunWriteDictionaryTest(FetchType::kFetchApiFromSharedWorker,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       FetchDictionaryFromServiceWorker) {
  RunWriteDictionaryTest(FetchType::kFetchApiFromServiceWorker,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       FetchDictionaryUingIframeNavigation) {
  RunWriteDictionaryTest(FetchType::kIframeNavigation,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test_dict.html"),
                         /*expect_success=*/false);
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       CrossOriginLinkRelCompressionDictionary) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetCrossOriginURL("/shared_dictionary/test.dict"));
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       CrossOriginFetchDictionary) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetCrossOriginURL("/shared_dictionary/test.dict"));
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       CrossOriginLinkRelCompressionDictionaryWithoutACAO) {
  RunWriteDictionaryTest(
      FetchType::kLinkRelCompressionDictionary,
      GetURL("/shared_dictionary/blank.html"),
      GetCrossOriginURL("/shared_dictionary/test_no_acao.dict"),
      /*expect_success=*/false);
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       CrossOriginFetchDictionaryWithoutACAO) {
  RunWriteDictionaryTest(
      FetchType::kFetchApi, GetURL("/shared_dictionary/blank.html"),
      GetCrossOriginURL("/shared_dictionary/test_no_acao.dict"),
      /*expect_success=*/false);
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       CrossOriginFetchDictionaryWithNoCorsMode) {
  RunWriteDictionaryTest(FetchType::kFetchApiWithNoCorsMode,
                         GetURL("/shared_dictionary/blank.html"),
                         GetCrossOriginURL("/shared_dictionary/test.dict"),
                         /*expect_success=*/false);
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       CrossOriginFetchDictionaryUingIframeNavigation) {
  RunWriteDictionaryTest(FetchType::kIframeNavigation,
                         GetURL("/shared_dictionary/blank.html"),
                         GetCrossOriginURL("/shared_dictionary/test_dict.html"),
                         /*expect_success=*/false);
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       SameOriginModeRequestSameOriginResource) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  EXPECT_EQ(kCompressedDataOriginalString,
            FetchSameOriginRequest(GetURL(kTestPath)));
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       NoCorsModeRequestSameOriginResource) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScript(GetURL(kTestPath + "?1")))
      << "Same origin resource";

  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScript(GURL(GetURL("/redirect?").spec() +
                                GetURL(kTestPath + "?2").spec())))
      << "Redirected from same origin to same origin resource";

  EXPECT_EQ(kUncompressedDataResultString,
            LoadTestScript(GURL(GetCrossOriginURL("/redirect?").spec() +
                                GetURL(kTestPath + "?3").spec())))
      << "Redirected from cross origin to same origin resource";

  EXPECT_EQ(kUncompressedDataResultString,
            LoadTestScript(GURL(GetURL("/redirect?").spec() +
                                GetCrossOriginURL("/redirect?").spec() +
                                GetURL(kTestPath + "?4").spec())))
      << "Redirected from same origin via cross origin to same origin resource";
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       NoCorsModeRequestCrossOriginResource) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetCrossOriginURL("/shared_dictionary/test.dict"));

  EXPECT_EQ(kUncompressedDataResultString,
            LoadTestScript(GetCrossOriginURL(kTestPath + "?1")))
      << "Cross origin resource";

  EXPECT_EQ(kUncompressedDataResultString,
            LoadTestScript(GURL(GetURL("/redirect?").spec() +
                                GetCrossOriginURL(kTestPath + "?2").spec())))
      << "Redirected from same origin to cross origin resource";

  EXPECT_EQ(kUncompressedDataResultString,
            LoadTestScript(GURL(GetCrossOriginURL("/redirect?").spec() +
                                GetCrossOriginURL(kTestPath + "?3").spec())))
      << "Redirected from cross origin to cross origin resource";

  EXPECT_EQ(kUncompressedDataResultString,
            LoadTestScript(GURL(GetCrossOriginURL("/redirect?").spec() +
                                GetURL("/redirect?").spec() +
                                GetCrossOriginURL(kTestPath + "?4").spec())))
      << "Redirected from cross origin via same origin to cross origin "
         "resource";
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       CorsModeRequestSameOriginResource) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScriptWithCrossOriginAnonymous(GetURL(kTestPath + "?1")))
      << "Same origin resource";
  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScriptWithCrossOriginAnonymous(GURL(
                GetURL("/redirect?").spec() + GetURL(kTestPath + "?2").spec())))
      << "Redirected from same origin to same origin resource";
  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScriptWithCrossOriginAnonymous(
                GURL(GetCrossOriginURL("/redirect?").spec() +
                     GetURL(kTestPath + "?3").spec())))
      << "Redirected from cross origin to same origin resource";
  EXPECT_EQ(
      kCompressedDataResultString,
      LoadTestScriptWithCrossOriginAnonymous(GURL(
          GetURL("/redirect?").spec() + GetCrossOriginURL("/redirect?").spec() +
          GetURL(kTestPath + "?4").spec())))
      << "Redirected from same origin via cross origin to same origin resource";
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       CorModeRequestCrossOriginResource) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetCrossOriginURL("/shared_dictionary/test.dict"));

  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScriptWithCrossOriginAnonymous(
                GetCrossOriginURL(kTestPath + "?1")))
      << "Cross origin resource";
  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScriptWithCrossOriginAnonymous(
                GURL(GetURL("/redirect?").spec() +
                     GetCrossOriginURL(kTestPath + "?2").spec())))
      << "Redirected from same origin to cross origin resource";
  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScriptWithCrossOriginAnonymous(
                GURL(GetCrossOriginURL("/redirect?").spec() +
                     GetCrossOriginURL(kTestPath + "?3").spec())))
      << "Redirected from cross origin to cross origin resource";
  EXPECT_EQ(
      kCompressedDataResultString,
      LoadTestScriptWithCrossOriginAnonymous(GURL(
          GetCrossOriginURL("/redirect?").spec() + GetURL("/redirect?").spec() +
          GetCrossOriginURL(kTestPath + "?4").spec())))
      << "Redirected from cross origin via same origin to cross origin "
         "resource";
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       CorsModeRequestWithCredentialsSameOriginResource) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  EXPECT_EQ(
      kCompressedDataResultString,
      LoadTestScriptWithCrossOriginUseCredentials(GetURL(kTestPath + "?1")))
      << "Same origin resource";
  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScriptWithCrossOriginUseCredentials(GURL(
                GetURL("/redirect?").spec() + GetURL(kTestPath + "?2").spec())))
      << "Redirected from same origin to same origin resource";
  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScriptWithCrossOriginUseCredentials(
                GURL(GetCrossOriginURL("/redirect?").spec() +
                     GetURL(kTestPath + "?3").spec())))
      << "Redirected from cross origin to same origin resource";
  EXPECT_EQ(
      kCompressedDataResultString,
      LoadTestScriptWithCrossOriginUseCredentials(GURL(
          GetURL("/redirect?").spec() + GetCrossOriginURL("/redirect?").spec() +
          GetURL(kTestPath + "?4").spec())))
      << "Redirected from same origin via cross origin to same origin resource";
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       CorModeRequestWithCredentialsCrossOriginResource) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetCrossOriginURL("/shared_dictionary/test.dict"));

  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScriptWithCrossOriginUseCredentials(
                GetCrossOriginURL(kTestPath + "?1")))
      << "Cross origin resource";
  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScriptWithCrossOriginUseCredentials(
                GURL(GetURL("/redirect?").spec() +
                     GetCrossOriginURL(kTestPath + "?2").spec())))
      << "Redirected from same origin to cross origin resource";
  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScriptWithCrossOriginUseCredentials(
                GURL(GetCrossOriginURL("/redirect?").spec() +
                     GetCrossOriginURL(kTestPath + "?3").spec())))
      << "Redirected from cross origin to cross origin resource";
  EXPECT_EQ(
      kCompressedDataResultString,
      LoadTestScriptWithCrossOriginUseCredentials(GURL(
          GetCrossOriginURL("/redirect?").spec() + GetURL("/redirect?").spec() +
          GetCrossOriginURL(kTestPath + "?4").spec())))
      << "Redirected from cross origin via same origin to cross origin "
         "resource";
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest, Navigation) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  const GURL target_url = GetURL(kTestPath + "?html");

  base::RunLoop loop;
  auto observer = std::make_unique<SharedDictionaryAccessObserver>(
      GetTargetShell()->web_contents(), loop.QuitClosure());
  EXPECT_TRUE(NavigateToURL(GetTargetShell(), target_url));
  loop.Run();

  ASSERT_TRUE(observer->details());
  EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kRead,
            observer->details()->type);
  EXPECT_EQ(target_url, observer->details()->url);
  EXPECT_EQ(net::SharedDictionaryIsolationKey(url::Origin::Create(target_url),
                                              net::SchemefulSite(target_url)),
            observer->details()->isolation_key);
  EXPECT_FALSE(observer->details()->is_blocked);

  EXPECT_EQ(kCompressedDataOriginalString,
            EvalJs(GetTargetShell()->web_contents()->GetPrimaryMainFrame(),
                   "document.body.innerText")
                .ExtractString());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       NavigationAfterSameOriginRedirect) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  const GURL target_url = GetURL(kTestPath + "?html");
  const GURL redirect_url =
      GURL(GetURL("/redirect?").spec() + GetURL(kTestPath + "?html").spec());

  base::RunLoop loop;
  auto observer = std::make_unique<SharedDictionaryAccessObserver>(
      GetTargetShell()->web_contents(), loop.QuitClosure());
  EXPECT_TRUE(NavigateToURL(GetTargetShell(), redirect_url, target_url));
  loop.Run();

  ASSERT_TRUE(observer->details());
  EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kRead,
            observer->details()->type);
  EXPECT_EQ(target_url, observer->details()->url);
  EXPECT_EQ(net::SharedDictionaryIsolationKey(url::Origin::Create(target_url),
                                              net::SchemefulSite(target_url)),
            observer->details()->isolation_key);
  EXPECT_FALSE(observer->details()->is_blocked);

  EXPECT_EQ(kCompressedDataOriginalString,
            EvalJs(GetTargetShell()->web_contents()->GetPrimaryMainFrame(),
                   "document.body.innerText")
                .ExtractString());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       NavigationAfterCrossOriginRedirect) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  const GURL target_url = GetURL(kTestPath + "?html");
  const GURL redirect_url = GURL(GetCrossOriginURL("/redirect?").spec() +
                                 GetURL(kTestPath + "?html").spec());

  base::RunLoop loop;
  auto observer = std::make_unique<SharedDictionaryAccessObserver>(
      GetTargetShell()->web_contents(), loop.QuitClosure());
  EXPECT_TRUE(NavigateToURL(GetTargetShell(), redirect_url, target_url));
  loop.Run();

  ASSERT_TRUE(observer->details());
  EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kRead,
            observer->details()->type);
  EXPECT_EQ(target_url, observer->details()->url);
  EXPECT_EQ(net::SharedDictionaryIsolationKey(url::Origin::Create(target_url),
                                              net::SchemefulSite(target_url)),
            observer->details()->isolation_key);
  EXPECT_FALSE(observer->details()->is_blocked);

  EXPECT_EQ(kCompressedDataOriginalString,
            EvalJs(GetTargetShell()->web_contents()->GetPrimaryMainFrame(),
                   "document.body.innerText")
                .ExtractString());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest, IframeNavigation) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  const GURL target_url = GetURL(kTestPath + "?html");

  base::RunLoop loop;
  auto observer = std::make_unique<SharedDictionaryAccessObserver>(
      GetTargetShell()->web_contents(), loop.QuitClosure());

  EXPECT_EQ(kCompressedDataOriginalString, LoadTestIframe(target_url));
  loop.Run();

  ASSERT_TRUE(observer->details());
  EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kRead,
            observer->details()->type);
  EXPECT_EQ(target_url, observer->details()->url);
  EXPECT_EQ(net::SharedDictionaryIsolationKey(url::Origin::Create(target_url),
                                              net::SchemefulSite(target_url)),
            observer->details()->isolation_key);
  EXPECT_FALSE(observer->details()->is_blocked);
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       IframeNavigationAfterSameOriginRedirect) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  const GURL target_url = GetURL(kTestPath + "?html");
  const GURL redirect_url =
      GURL(GetURL("/redirect?").spec() + GetURL(kTestPath + "?html").spec());

  base::RunLoop loop;
  auto observer = std::make_unique<SharedDictionaryAccessObserver>(
      GetTargetShell()->web_contents(), loop.QuitClosure());

  EXPECT_EQ(kCompressedDataOriginalString, LoadTestIframe(redirect_url));
  loop.Run();

  ASSERT_TRUE(observer->details());
  EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kRead,
            observer->details()->type);
  EXPECT_EQ(target_url, observer->details()->url);
  EXPECT_EQ(net::SharedDictionaryIsolationKey(url::Origin::Create(target_url),
                                              net::SchemefulSite(target_url)),
            observer->details()->isolation_key);
  EXPECT_FALSE(observer->details()->is_blocked);
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       IframeNavigationAfterCrossOriginRedirect) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  const GURL target_url = GetURL(kTestPath + "?html");
  const GURL redirect_url = GURL(GetCrossOriginURL("/redirect?").spec() +
                                 GetURL(kTestPath + "?html").spec());

  base::RunLoop loop;
  auto observer = std::make_unique<SharedDictionaryAccessObserver>(
      GetTargetShell()->web_contents(), loop.QuitClosure());

  EXPECT_EQ(kCompressedDataOriginalString, LoadTestIframe(redirect_url));
  loop.Run();

  ASSERT_TRUE(observer->details());
  EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kRead,
            observer->details()->type);
  EXPECT_EQ(target_url, observer->details()->url);
  EXPECT_EQ(net::SharedDictionaryIsolationKey(url::Origin::Create(target_url),
                                              net::SchemefulSite(target_url)),
            observer->details()->isolation_key);
  EXPECT_FALSE(observer->details()->is_blocked);
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest, MatchDestEmptyString) {
  Shell* shell = GetTargetShell();
  EXPECT_TRUE(NavigateToURL(shell, GetURL("/shared_dictionary/blank.html")));

  // The response header contains `match-dest=("")` in Use-As-Dictionary header.
  const GURL dictionary_url = GetURL("/shared_dictionary/test.empty_dest.dict");
  EXPECT_TRUE(ExecJs(shell->web_contents()->GetPrimaryMainFrame(),
                     LinkRelCompressionDictionaryScript(dictionary_url)));

  // Wait for the dictionary to be registered.
  EXPECT_TRUE(WaitForHistogram(
      GetBrowserType() == BrowserType::kNormal
          ? "Net.SharedDictionaryManagerOnDisk.DictionarySizeKB"
          : "Net.SharedDictionaryWriterInMemory.DictionarySize"));

  // Check that Chrome uses the dictionary while fetching the resource using
  // Fetch API.
  EXPECT_EQ(kCompressedDataOriginalString,
            EvalJs(shell->web_contents()->GetPrimaryMainFrame(),
                   FetchTargetDataScript(dictionary_url))
                .ExtractString());

  // Check that Chrome doesn't use the dictionary while fetching a script.
  EXPECT_EQ(kUncompressedDataResultString,
            LoadTestScript(GetURL(kTestPath + "?for_script")));
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest, MatchDestScript) {
  Shell* shell = GetTargetShell();
  EXPECT_TRUE(NavigateToURL(shell, GetURL("/shared_dictionary/blank.html")));

  // The response header contains `match-dest=("script")` in Use-As-Dictionary
  // header.
  const GURL dictionary_url =
      GetURL("/shared_dictionary/test.script_dest.dict");
  EXPECT_TRUE(ExecJs(shell->web_contents()->GetPrimaryMainFrame(),
                     LinkRelCompressionDictionaryScript(dictionary_url)));

  // Wait for the dictionary to be registered.
  EXPECT_TRUE(WaitForHistogram(
      GetBrowserType() == BrowserType::kNormal
          ? "Net.SharedDictionaryManagerOnDisk.DictionarySizeKB"
          : "Net.SharedDictionaryWriterInMemory.DictionarySize"));

  // Check that Chrome uses the dictionary while fetching a script.
  EXPECT_EQ(kCompressedDataResultString,
            LoadTestScript(GetURL(kTestPath + "?for_script")));

  // Check that Chrome doesn't use the dictionary while fetching the resource
  // using Fetch API.
  EXPECT_EQ(kUncompressedDataString,
            EvalJs(shell->web_contents()->GetPrimaryMainFrame(),
                   FetchTargetDataScript(dictionary_url))
                .ExtractString());
}

IN_PROC_BROWSER_TEST_P(
    SharedDictionaryBrowserTest,
    GetUsageInfoAndClearSharedDictionaryCacheForIsolationKey) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  net::SharedDictionaryIsolationKey isolation_key =
      net::SharedDictionaryIsolationKey(url::Origin::Create(GetURL("/")),
                                        net::SchemefulSite(GetURL("/")));

  EXPECT_THAT(GetSharedDictionaryUsageInfo(GetTargetShell()),
              UnorderedElementsAreArray({net::SharedDictionaryUsageInfo{
                  .isolation_key = isolation_key,
                  .total_size_bytes = static_cast<uint64_t>(
                      GetTestDataFileSize("shared_dictionary/test.dict"))}}));
  {
    base::RunLoop loop;
    GetTargetNetworkContext()->ClearSharedDictionaryCacheForIsolationKey(
        isolation_key, loop.QuitClosure());
    loop.Run();
  }
  EXPECT_TRUE(GetSharedDictionaryUsageInfo(GetTargetShell()).empty());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest, GetTotalSizeAndOrigins) {
  base::Time time1 = base::Time::Now();
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
  base::Time time2 = base::Time::Now();

  EXPECT_TRUE(
      GetOriginsBetween(GetTargetShell(), time1 - base::Seconds(1), time1)
          .empty());
  EXPECT_THAT(GetOriginsBetween(GetTargetShell(), time1, time2),
              testing::ElementsAreArray({url::Origin::Create(GetURL("/"))}));
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest, GetSharedDictionaryInfo) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  net::SharedDictionaryIsolationKey isolation_key =
      net::SharedDictionaryIsolationKey(url::Origin::Create(GetURL("/")),
                                        net::SchemefulSite(GetURL("/")));
  {
    base::RunLoop loop;
    GetTargetNetworkContext()->GetSharedDictionaryInfo(
        isolation_key,
        base::BindLambdaForTesting(
            [&](std::vector<network::mojom::SharedDictionaryInfoPtr> result) {
              ASSERT_EQ(1u, result.size());

              EXPECT_EQ("/shared_dictionary/path/*", result[0]->match);
              EXPECT_EQ(GetURL("/shared_dictionary/test.dict"),
                        result[0]->dictionary_url);
              EXPECT_EQ(static_cast<uint64_t>(
                            GetTestDataFileSize("shared_dictionary/test.dict")),
                        result[0]->size);
              EXPECT_EQ(kExpectedDictionaryHashValue, result[0]->hash);
              loop.Quit();
            }));
    loop.Run();
  }
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest, ClearSiteData) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
  base::RunLoop loop;
  content::ClearSiteData(
      GetTargetShell()->web_contents()->GetBrowserContext()->GetWeakPtr(),
      /*storage_partition_config=*/std::nullopt,
      /*origin=*/url::Origin::Create(GetURL("/")),
      content::ClearSiteDataTypeSet::All(),
      /*storage_buckets_to_remove=*/{},
      /*avoid_closing_connections=*/true,
      /*cookie_partition_key=*/std::nullopt,
      /*storage_key=*/std::nullopt,
      /*partitioned_state_allowed_only=*/false,
      /*callback=*/loop.QuitClosure());
  loop.Run();

  EXPECT_TRUE(GetSharedDictionaryUsageInfo(GetTargetShell()).empty());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       ClearSiteDataNavigationCacheDirective) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
  EXPECT_EQ(1u, GetSharedDictionaryUsageInfo(GetTargetShell()).size());
  EXPECT_TRUE(NavigateToURL(
      GetTargetShell(), GetURL("/shared_dictionary/clear_site_data?cache")));
  // Navigation to a page which HTTP response header contains
  // `Clear-Site-Data: "cache"` should clear the shared dictionary.
  EXPECT_TRUE(GetSharedDictionaryUsageInfo(GetTargetShell()).empty());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       ClearSiteDataNavigationCookiesDirective) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
  EXPECT_EQ(1u, GetSharedDictionaryUsageInfo(GetTargetShell()).size());
  EXPECT_TRUE(NavigateToURL(
      GetTargetShell(), GetURL("/shared_dictionary/clear_site_data?cookies")));
  // Navigation to a page which HTTP response header contains
  // `Clear-Site-Data: "cookies"` should clear the shared dictionary.
  EXPECT_TRUE(GetSharedDictionaryUsageInfo(GetTargetShell()).empty());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       ClearSiteDataNavigationStorageDirective) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
  EXPECT_EQ(1u, GetSharedDictionaryUsageInfo(GetTargetShell()).size());
  EXPECT_TRUE(NavigateToURL(
      GetTargetShell(), GetURL("/shared_dictionary/clear_site_data?storage")));
  // Navigation to a page which HTTP response header contains
  // `Clear-Site-Data: "storage"` should NOT clear the shared dictionary.
  EXPECT_EQ(1u, GetSharedDictionaryUsageInfo(GetTargetShell()).size());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       ClearSiteDataFetchCacheDirective) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
  EXPECT_EQ(1u, GetSharedDictionaryUsageInfo(GetTargetShell()).size());
  EXPECT_TRUE(
      ExecJs(GetTargetShell(),
             JsReplace("fetch($1);",
                       GetURL("/shared_dictionary/clear_site_data?cache"))));
  // Fetching a resource which HTTP response header contains
  // `Clear-Site-Data: "cache"` should clear the shared dictionary.
  EXPECT_TRUE(GetSharedDictionaryUsageInfo(GetTargetShell()).empty());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       ClearSiteDataFetchCookiesDirective) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
  EXPECT_EQ(1u, GetSharedDictionaryUsageInfo(GetTargetShell()).size());
  EXPECT_TRUE(
      ExecJs(GetTargetShell(),
             JsReplace("fetch($1);",
                       GetURL("/shared_dictionary/clear_site_data?cookies"))));
  // Fetching a resource which HTTP response header contains
  // `Clear-Site-Data: "cookies"` should clear the shared dictionary.
  EXPECT_TRUE(GetSharedDictionaryUsageInfo(GetTargetShell()).empty());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       ClearSiteDataFetchStorageDirective) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));
  EXPECT_EQ(1u, GetSharedDictionaryUsageInfo(GetTargetShell()).size());
  EXPECT_TRUE(
      ExecJs(GetTargetShell(),
             JsReplace("fetch($1);",
                       GetURL("/shared_dictionary/clear_site_data?storage"))));
  // Fetching a resource which HTTP response header contains
  // `Clear-Site-Data: "storage"` should NOT clear the shared dictionary.
  EXPECT_EQ(1u, GetSharedDictionaryUsageInfo(GetTargetShell()).size());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       ClearSiteDataCrossOriginFetchCacheDirective) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetCrossOriginURL("/shared_dictionary/test.dict"));
  EXPECT_EQ(1u, GetSharedDictionaryUsageInfo(GetTargetShell()).size());
  // Setting the credentials flag because Clear-Site-Data header handling works
  // only when credentials flag is set.
  EXPECT_TRUE(ExecJs(
      GetTargetShell(),
      JsReplace(
          "fetch($1, {credentials: 'include'});",
          GetCrossOriginURL("/shared_dictionary/clear_site_data?cache"))));
  // Fetching a cross origin resource which HTTP response header contains
  // `Clear-Site-Data: "cache"` should clear the shared dictionary.
  EXPECT_TRUE(GetSharedDictionaryUsageInfo(GetTargetShell()).empty());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       ClearSiteDataCrossOriginFetchCookiesDirective) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetCrossOriginURL("/shared_dictionary/test.dict"));
  EXPECT_EQ(1u, GetSharedDictionaryUsageInfo(GetTargetShell()).size());
  // Setting the credentials flag because Clear-Site-Data header handling works
  // only when credentials flag is set.
  EXPECT_TRUE(ExecJs(
      GetTargetShell(),
      JsReplace(
          "fetch($1, {credentials: 'include'});",
          GetCrossOriginURL("/shared_dictionary/clear_site_data?cookies"))));
  // Fetching a cross origin resource which HTTP response header contains
  // `Clear-Site-Data: "cookies"` should clear the shared dictionary.
  EXPECT_TRUE(GetSharedDictionaryUsageInfo(GetTargetShell()).empty());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       ClearSiteDataCrossOriginFetchStorageDirective) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetCrossOriginURL("/shared_dictionary/test.dict"));
  EXPECT_EQ(1u, GetSharedDictionaryUsageInfo(GetTargetShell()).size());
  // Setting the credentials flag because Clear-Site-Data header handling works
  // only when credentials flag is set.
  EXPECT_TRUE(ExecJs(
      GetTargetShell(),
      JsReplace(
          "fetch($1, {credentials: 'include'});",
          GetCrossOriginURL("/shared_dictionary/clear_site_data?storage"))));
  // Fetching a cross origin resource which HTTP response header contains
  // `Clear-Site-Data: "storage"` should NOT clear the shared dictionary.
  EXPECT_EQ(1u, GetSharedDictionaryUsageInfo(GetTargetShell()).size());
}

// TODO(crbug.com/40599527): When we support wildcard directive
// `Clear-Site-Data: "*"", add test for it.

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       DictionaryReadCountForNavigation) {
  if (GetBrowserType() == BrowserType::kOffTheRecord) {
    // We want to test the behavior of SharedDictionaryStorageOnDisk.
    return;
  }
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(NavigateToURL(shell(), GetURL("/shared_dictionary/blank.html")));
  const std::string histogram_name =
      "Net.SharedDictionaryStorageOnDisk.MetadataReadTime.Empty";

  EXPECT_TRUE(WaitForHistogram(histogram_name));
  histogram_tester.ExpectTotalCount(histogram_name,
                                    /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest, RestartWithAuth) {
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  DummyAuthContentBrowserClient browser_client;

  EXPECT_FALSE(browser_client.create_login_delegate_called());
  ASSERT_TRUE(NavigateToURL(GetTargetShell(), GetURL(kHttpAuthPath)));
  EXPECT_TRUE(browser_client.create_login_delegate_called());
  EXPECT_EQ(kCompressedDataOriginalString,
            EvalJs(GetTargetShell()->web_contents()->GetPrimaryMainFrame(),
                   "document.body.innerText")
                .ExtractString());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       RestartedAfterCertErrorPageUseSbr) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  RegisterTestRequestHandler(https_server);
  ASSERT_TRUE(https_server.Start());
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         https_server.GetURL("/shared_dictionary/blank.html"),
                         https_server.GetURL("/shared_dictionary/test.dict"));

  // Resetting the SSL config of the server to trigger a certificate error.
  ASSERT_TRUE(https_server.ResetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED,
                                          net::SSLServerConfig()));

  CertificateErrorAllowingContentBrowserClient browser_client;
  EXPECT_FALSE(browser_client.allow_certificate_error_called());
  EXPECT_TRUE(NavigateToURL(GetTargetShell(),
                            https_server.GetURL(kTestPath + "?html")));
  EXPECT_TRUE(browser_client.allow_certificate_error_called());

  EXPECT_EQ(kCompressedDataOriginalString,
            EvalJs(GetTargetShell()->web_contents()->GetPrimaryMainFrame(),
                   "document.body.innerText")
                .ExtractString());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       RestartWithCertificatePageUseSbr) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  RegisterTestRequestHandler(https_server);
  ASSERT_TRUE(https_server.Start());
  RunWriteDictionaryTest(FetchType::kFetchApi,
                         https_server.GetURL("/shared_dictionary/blank.html"),
                         https_server.GetURL("/shared_dictionary/test.dict"));

  // Resetting the SSL config of the server to require a client certificate.
  net::SSLServerConfig server_config;
  server_config.client_cert_type = net::SSLServerConfig::REQUIRE_CLIENT_CERT;
  ASSERT_TRUE(https_server.ResetSSLConfig(net::EmbeddedTestServer::CERT_OK,
                                          server_config));

  DummyClientCertStoreContentBrowserClient browser_client;
  EXPECT_FALSE(browser_client.select_client_certificate_called());
  EXPECT_TRUE(NavigateToURL(GetTargetShell(),
                            https_server.GetURL(kTestPath + "?html")));
  EXPECT_TRUE(browser_client.select_client_certificate_called());
  EXPECT_EQ(kCompressedDataOriginalString,
            EvalJs(GetTargetShell()->web_contents()->GetPrimaryMainFrame(),
                   "document.body.innerText")
                .ExtractString());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       EncodedBodySizeAndDecodedBodySize) {
  RunWriteDictionaryTest(FetchType::kLinkRelCompressionDictionary,
                         GetURL("/shared_dictionary/blank.html"),
                         GetURL("/shared_dictionary/test.dict"));

  EXPECT_EQ(base::StringPrintf("%zu, %zu", kZstdCompressedDataString.size(),
                               kCompressedDataOriginalString.size()),
            EvalJs(GetTargetShell()->web_contents()->GetPrimaryMainFrame(),
                   JsReplace(R"(
          (async () => {
            const targetUrl = $1;
            const promise = new Promise((resolve) => {
              const observer = new PerformanceObserver((list) => {
                list.getEntries().forEach((entry) => {
                  if (entry.name == targetUrl) {
                    resolve(entry);
                  }
                });
              });
              observer.observe({ type: 'resource', buffered: true });
            });
            fetch(targetUrl);
            const entry = await promise;
            return entry.encodedBodySize + ', ' + entry.decodedBodySize;
          })();
        )",
                             GetURL("/shared_dictionary/path/test?")))
                .ExtractString());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       PreloadSharedDictionaryInfo) {
  mojo::PendingRemote<network::mojom::PreloadedSharedDictionaryInfoHandle>
      preloaded_shared_dictionaries_handle;
  GetTargetNetworkContext()->PreloadSharedDictionaryInfoForDocument(
      {GetURL("/")},
      preloaded_shared_dictionaries_handle.InitWithNewPipeAndPassReceiver());
  EXPECT_TRUE(HasPreloadedSharedDictionaryInfo());
  preloaded_shared_dictionaries_handle.reset();
  EXPECT_FALSE(HasPreloadedSharedDictionaryInfo());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       DoNotPreloadDictionayUnderMemoryPressure) {
  SendMemoryPressureToNetworkService();
  mojo::PendingRemote<network::mojom::PreloadedSharedDictionaryInfoHandle>
      preloaded_shared_dictionaries_handle;
  GetTargetNetworkContext()->PreloadSharedDictionaryInfoForDocument(
      {GetURL("/")},
      preloaded_shared_dictionaries_handle.InitWithNewPipeAndPassReceiver());
  EXPECT_FALSE(HasPreloadedSharedDictionaryInfo());
}

IN_PROC_BROWSER_TEST_P(SharedDictionaryBrowserTest,
                       PreloadedDictionayDiscardedByMemoryPressure) {
  mojo::PendingRemote<network::mojom::PreloadedSharedDictionaryInfoHandle>
      preloaded_shared_dictionaries_handle;
  GetTargetNetworkContext()->PreloadSharedDictionaryInfoForDocument(
      {GetURL("/")},
      preloaded_shared_dictionaries_handle.InitWithNewPipeAndPassReceiver());
  EXPECT_TRUE(HasPreloadedSharedDictionaryInfo());
  SendMemoryPressureToNetworkService();
  EXPECT_FALSE(HasPreloadedSharedDictionaryInfo());
}

}  // namespace

}  // namespace content
