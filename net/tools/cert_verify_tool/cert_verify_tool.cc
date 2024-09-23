// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string_view>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_split.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/crl_set.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/internal/platform_trust_store.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/x509_util.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/tools/cert_verify_tool/cert_verify_tool_util.h"
#include "net/tools/cert_verify_tool/verify_using_cert_verify_proc.h"
#include "net/tools/cert_verify_tool/verify_using_path_builder.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/boringssl/src/pki/trust_store.h"
#include "third_party/boringssl/src/pki/trust_store_collection.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/cert/internal/trust_store_chrome.h"
#endif

namespace {

enum class RootStoreType {
  // No roots other than those explicitly passed in on the command line.
  kEmpty,
#if !BUILDFLAG(CHROME_ROOT_STORE_ONLY)
  // Use the system root store.
  kSystem,
#endif
  // Use the Chrome Root Store.
  kChrome
};

std::string GetUserAgent() {
  return "cert_verify_tool/0.1";
}

void SetUpOnNetworkThread(
    std::unique_ptr<net::URLRequestContext>* context,
    scoped_refptr<net::CertNetFetcherURLRequest>* cert_net_fetcher,
    base::WaitableEvent* initialization_complete_event) {
  net::URLRequestContextBuilder url_request_context_builder;
  url_request_context_builder.set_user_agent(GetUserAgent());
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // On Linux, use a fixed ProxyConfigService, since the default one
  // depends on glib.
  //
  // TODO(akalin): Remove this once http://crbug.com/146421 is fixed.
  url_request_context_builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation()));
#endif
  *context = url_request_context_builder.Build();

  // TODO(mattm): add command line flag to configure using
  // CertNetFetcher
  *cert_net_fetcher = base::MakeRefCounted<net::CertNetFetcherURLRequest>();
  (*cert_net_fetcher)->SetURLRequestContext(context->get());
  initialization_complete_event->Signal();
}

void ShutdownOnNetworkThread(
    std::unique_ptr<net::URLRequestContext>* context,
    scoped_refptr<net::CertNetFetcherURLRequest>* cert_net_fetcher) {
  (*cert_net_fetcher)->Shutdown();
  cert_net_fetcher->reset();
  context->reset();
}

// Base class to abstract running a particular implementation of certificate
// verification.
class CertVerifyImpl {
 public:
  virtual ~CertVerifyImpl() = default;

  virtual std::string GetName() const = 0;

  // Does certificate verification.
  //
  // Note that |hostname| may be empty to indicate that no name validation is
  // requested, and a null value of |verify_time| means to use the current time.
  virtual bool VerifyCert(const CertInput& target_der_cert,
                          const std::string& hostname,
                          const std::vector<CertInput>& intermediate_der_certs,
                          const std::vector<CertInputWithTrustSetting>&
                              der_certs_with_trust_settings,
                          base::Time verify_time,
                          net::CRLSet* crl_set,
                          const base::FilePath& dump_prefix_path) = 0;
};

// Runs certificate verification using a particular CertVerifyProc.
class CertVerifyImplUsingProc : public CertVerifyImpl {
 public:
  CertVerifyImplUsingProc(const std::string& name,
                          scoped_refptr<net::CertVerifyProc> proc)
      : name_(name), proc_(std::move(proc)) {}

  std::string GetName() const override { return name_; }

  bool VerifyCert(const CertInput& target_der_cert,
                  const std::string& hostname,
                  const std::vector<CertInput>& intermediate_der_certs,
                  const std::vector<CertInputWithTrustSetting>&
                      der_certs_with_trust_settings,
                  base::Time verify_time,
                  net::CRLSet* crl_set,
                  const base::FilePath& dump_prefix_path) override {
    if (!verify_time.is_null()) {
      std::cerr << "WARNING: --time is not supported by " << GetName()
                << ", will use current time.\n";
    }

    if (hostname.empty()) {
      std::cerr << "ERROR: --hostname is required for " << GetName()
                << ", skipping\n";
      return true;  // "skipping" is considered a successful return.
    }

    base::FilePath dump_path;
    if (!dump_prefix_path.empty()) {
      dump_path = dump_prefix_path.AddExtension(FILE_PATH_LITERAL(".pem"))
                      .InsertBeforeExtensionASCII("." + GetName());
    }

    return VerifyUsingCertVerifyProc(proc_.get(), target_der_cert, hostname,
                                     intermediate_der_certs,
                                     der_certs_with_trust_settings, dump_path);
  }

 private:
  const std::string name_;
  scoped_refptr<net::CertVerifyProc> proc_;
};

// Runs certificate verification using bssl::CertPathBuilder.
class CertVerifyImplUsingPathBuilder : public CertVerifyImpl {
 public:
  explicit CertVerifyImplUsingPathBuilder(
      scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
      std::unique_ptr<net::SystemTrustStore> system_trust_store)
      : cert_net_fetcher_(std::move(cert_net_fetcher)),
        system_trust_store_(std::move(system_trust_store)) {}

  std::string GetName() const override { return "CertPathBuilder"; }

  bool VerifyCert(const CertInput& target_der_cert,
                  const std::string& hostname,
                  const std::vector<CertInput>& intermediate_der_certs,
                  const std::vector<CertInputWithTrustSetting>&
                      der_certs_with_trust_settings,
                  base::Time verify_time,
                  net::CRLSet* crl_set,
                  const base::FilePath& dump_prefix_path) override {
    if (!hostname.empty()) {
      std::cerr << "WARNING: --hostname is not verified with CertPathBuilder\n";
    }

    if (verify_time.is_null()) {
      verify_time = base::Time::Now();
    }

    return VerifyUsingPathBuilder(target_der_cert, intermediate_der_certs,
                                  der_certs_with_trust_settings, verify_time,
                                  dump_prefix_path, cert_net_fetcher_,
                                  system_trust_store_.get());
  }

 private:
  scoped_refptr<net::CertNetFetcher> cert_net_fetcher_;
  std::unique_ptr<net::SystemTrustStore> system_trust_store_;
};

class DummySystemTrustStore : public net::SystemTrustStore {
 public:
  bssl::TrustStore* GetTrustStore() override { return &trust_store_; }

  bool IsKnownRoot(const bssl::ParsedCertificate* trust_anchor) const override {
    return false;
  }

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  net::PlatformTrustStore* GetPlatformTrustStore() override { return nullptr; }

  bool IsLocallyTrustedRoot(
      const bssl::ParsedCertificate* trust_anchor) override {
    return false;
  }

  int64_t chrome_root_store_version() const override { return 0; }

  base::span<const net::ChromeRootCertConstraints> GetChromeRootConstraints(
      const bssl::ParsedCertificate* cert) const override {
    return {};
  }
#endif

 private:
  bssl::TrustStoreCollection trust_store_;
};

std::unique_ptr<net::SystemTrustStore> CreateSystemTrustStore(
    std::string_view impl_name,
    RootStoreType root_store_type) {
  switch (root_store_type) {
#if BUILDFLAG(IS_FUCHSIA)
    case RootStoreType::kSystem:
      std::cerr << impl_name
                << ": using system roots (--roots are in addition).\n";
      return net::CreateSslSystemTrustStore();
#endif
    case RootStoreType::kChrome:
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
      std::cerr << impl_name
                << ": using Chrome Root Store (--roots are in addition).\n";
      return net::CreateSslSystemTrustStoreChromeRoot(
          std::make_unique<net::TrustStoreChrome>());
#else
      std::cerr << impl_name << ": not supported.\n";
      [[fallthrough]];
#endif

    case RootStoreType::kEmpty:
    default:
      std::cerr << impl_name << ": only using --roots specified.\n";
      return std::make_unique<DummySystemTrustStore>();
  }
}

// Creates an subclass of CertVerifyImpl based on its name, or returns nullptr.
std::unique_ptr<CertVerifyImpl> CreateCertVerifyImplFromName(
    std::string_view impl_name,
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
    scoped_refptr<net::CRLSet> crl_set,
    RootStoreType root_store_type) {
#if !(BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(CHROME_ROOT_STORE_ONLY))
  if (impl_name == "platform") {
    if (root_store_type != RootStoreType::kSystem) {
      std::cerr << "WARNING: platform verifier not supported with "
                   "--no-system-roots and --use-chrome-root-store, using "
                   "system roots (--roots are in addition).\n";
    }

    return std::make_unique<CertVerifyImplUsingProc>(
        "CertVerifyProc (system)",
        net::CertVerifyProc::CreateSystemVerifyProc(std::move(cert_net_fetcher),
                                                    std::move(crl_set)));
  }
#endif

  if (impl_name == "builtin") {
    return std::make_unique<CertVerifyImplUsingProc>(
        "CertVerifyProcBuiltin",
        net::CreateCertVerifyProcBuiltin(
            std::move(cert_net_fetcher), std::move(crl_set),
            // TODO(crbug.com/41392053): support CT.
            std::make_unique<net::DoNothingCTVerifier>(),
            base::MakeRefCounted<net::DefaultCTPolicyEnforcer>(),
            CreateSystemTrustStore(impl_name, root_store_type), {},
            std::nullopt));
  }

  if (impl_name == "pathbuilder") {
    return std::make_unique<CertVerifyImplUsingPathBuilder>(
        std::move(cert_net_fetcher),
        CreateSystemTrustStore(impl_name, root_store_type));
  }

  std::cerr << "WARNING: Unrecognized impl: " << impl_name << "\n";
  return nullptr;
}

void PrintCertHashAndSubject(CRYPTO_BUFFER* cert) {
  std::cout << " " << FingerPrintCryptoBuffer(cert) << " "
            << SubjectFromCryptoBuffer(cert) << "\n";
}

void PrintInputChain(const CertInput& target,
                     const std::vector<CertInput>& intermediates) {
  std::cout << "Input chain:\n";
  PrintCertHashAndSubject(
      net::x509_util::CreateCryptoBuffer(target.der_cert).get());
  for (const auto& intermediate : intermediates) {
    PrintCertHashAndSubject(
        net::x509_util::CreateCryptoBuffer(intermediate.der_cert).get());
  }
  std::cout << "\n";
}

void PrintAdditionalRoots(const std::vector<CertInputWithTrustSetting>&
                              der_certs_with_trust_settings) {
  std::cout << "Additional roots:\n";
  for (const auto& cert : der_certs_with_trust_settings) {
    std::cout << " " << cert.trust.ToDebugString() << ":\n ";
    PrintCertHashAndSubject(
        net::x509_util::CreateCryptoBuffer(cert.cert_input.der_cert).get());
  }
  std::cout << "\n";
}

const char kUsage[] =
    " [flags] <target/chain>\n"
    "\n"
    " <target/chain> is a file containing certificates [1]. Minimally it\n"
    " contains the target certificate. Optionally it may subsequently list\n"
    " additional certificates needed to build a chain (this is equivalent to\n"
    " specifying them through --intermediates)\n"
    "\n"
    "Flags:\n"
    "\n"
    " --hostname=<hostname>\n"
    "      The hostname required to match the end-entity certificate.\n"
    "      Required for the CertVerifyProc implementation.\n"
    "\n"
    " --roots=<certs path>\n"
    "      <certs path> is a file containing certificates [1] to interpret as\n"
    "      trust anchors (without any anchor constraints).\n"
    "\n"
    " --no-system-roots\n"
    "      Do not use system provided trust roots, only trust roots specified\n"
    "      by --roots or --trust-last-cert will be used. Only supported by\n"
    "      the builtin and pathbuilter impls.\n"
    "\n"
    " --use-chrome-root-store\n"
    "      Use the Chrome Root Store. Only supported by the builtin and \n"
    "      pathbuilder impls; if set will override the --no-system-roots \n"
    "      flag.\n"
    "\n"
    " --intermediates=<certs path>\n"
    "      <certs path> is a file containing certificates [1] for use when\n"
    "      path building is looking for intermediates.\n"
    "\n"
    " --impls=<ordered list of implementations>\n"
    "      Ordered list of the verifier implementations to run. If omitted,\n"
    "      will default to: \"platform,builtin,pathbuilder\".\n"
    "      Changing this can lead to different results in cases where the\n"
    "      platform verifier affects global caches (as in the case of NSS).\n"
    "\n"
    " --trust-last-cert\n"
    "      Removes the final intermediate from the chain and instead adds it\n"
    "      as a root. This is useful when providing a <target/chain>\n"
    "      parameter whose final certificate is a trust anchor.\n"
    "\n"
    " --root-trust=<trust string>\n"
    "      Roots trusted by --roots and --trust-last-cert will be trusted\n"
    "      with the specified trust [2].\n"
    "\n"
    " --trust-leaf-cert=[trust string]\n"
    "      The leaf cert will be considered trusted with the specified\n"
    "      trust [2]. If [trust string] is omitted, defaults to TRUSTED_LEAF.\n"
    "\n"
    " --time=<time>\n"
    "      Use <time> instead of the current system time. <time> is\n"
    "      interpreted in local time if a timezone is not specified.\n"
    "      Many common formats are supported, including:\n"
    "        1994-11-15 12:45:26 GMT\n"
    "        Tue, 15 Nov 1994 12:45:26 GMT\n"
    "        Nov 15 12:45:26 1994 GMT\n"
    "\n"
    " --crlset=<crlset path>\n"
    "      <crlset path> is a file containing a serialized CRLSet to use\n"
    "      during revocation checking. For example:\n"
    "        <chrome data dir>/CertificateRevocation/<number>/crl-set\n"
    "\n"
    " --dump=<file prefix>\n"
    "      Dumps the verified chain to PEM files starting with\n"
    "      <file prefix>.\n"
    "\n"
    "\n"
    "[1] A \"file containing certificates\" means a path to a file that can\n"
    "    either be:\n"
    "    * A binary file containing a single DER-encoded RFC 5280 Certificate\n"
    "    * A PEM file containing one or more CERTIFICATE blocks (DER-encoded\n"
    "      RFC 5280 Certificate)\n"
    "\n"
    "[2] A \"trust string\" consists of a trust type and zero or more options\n"
    "    separated by '+' characters. Note that these trust settings are only\n"
    "    honored by the builtin & pathbuilder impls.\n"
    "    Trust types: UNSPECIFIED, DISTRUSTED, TRUSTED_ANCHOR,\n"
    "                 TRUSTED_ANCHOR_OR_LEAF, TRUSTED_LEAF\n"
    "    Options: enforce_anchor_expiry, enforce_anchor_constraints,\n"
    "             require_anchor_basic_constraints, require_leaf_selfsigned\n"
    "    Ex: TRUSTED_ANCHOR+enforce_anchor_expiry+enforce_anchor_constraints\n";

void PrintUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0 << kUsage;

  // TODO(mattm): allow <certs path> to be a directory containing DER/PEM files?
  // TODO(mattm): allow target to specify an HTTPS URL to check the cert of?
  // TODO(mattm): allow target to be a verify_certificate_chain_unittest .test
  // file?
  // TODO(mattm): allow specifying ocsp_response and sct_list inputs as well.
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  if (!base::CommandLine::Init(argc, argv)) {
    std::cerr << "ERROR in CommandLine::Init\n";
    return 1;
  }
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("cert_verify_tool");
  absl::Cleanup cleanup = [] { base::ThreadPoolInstance::Get()->Shutdown(); };
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  base::CommandLine::StringVector args = command_line.GetArgs();
  if (args.size() != 1U || command_line.HasSwitch("help")) {
    PrintUsage(argv[0]);
    return 1;
  }

  std::string hostname = command_line.GetSwitchValueASCII("hostname");

  base::Time verify_time;
  std::string time_flag = command_line.GetSwitchValueASCII("time");
  if (!time_flag.empty()) {
    if (!base::Time::FromString(time_flag.c_str(), &verify_time)) {
      std::cerr << "Error parsing --time flag\n";
      return 1;
    }
  }

#if BUILDFLAG(CHROME_ROOT_STORE_ONLY)
  RootStoreType root_store_type = RootStoreType::kChrome;
#else
  RootStoreType root_store_type = RootStoreType::kSystem;
#endif

  if (command_line.HasSwitch("no-system-roots")) {
    root_store_type = RootStoreType::kEmpty;
  }
  if (command_line.HasSwitch("use-chrome-root-store")) {
    root_store_type = RootStoreType::kChrome;
  }

  base::FilePath roots_path = command_line.GetSwitchValuePath("roots");
  base::FilePath intermediates_path =
      command_line.GetSwitchValuePath("intermediates");
  base::FilePath target_path = base::FilePath(args[0]);

  base::FilePath crlset_path = command_line.GetSwitchValuePath("crlset");
  scoped_refptr<net::CRLSet> crl_set = net::CRLSet::BuiltinCRLSet();
  if (!crlset_path.empty()) {
    std::string crl_set_bytes;
    if (!ReadFromFile(crlset_path, &crl_set_bytes))
      return 1;
    if (!net::CRLSet::Parse(crl_set_bytes, &crl_set)) {
      std::cerr << "Error parsing CRLSet\n";
      return 1;
    }
  }

  base::FilePath dump_prefix_path = command_line.GetSwitchValuePath("dump");

  std::vector<CertInputWithTrustSetting> der_certs_with_trust_settings;
  std::vector<CertInput> root_der_certs;
  std::vector<CertInput> intermediate_der_certs;
  CertInput target_der_cert;

  if (!roots_path.empty())
    ReadCertificatesFromFile(roots_path, &root_der_certs);
  if (!intermediates_path.empty())
    ReadCertificatesFromFile(intermediates_path, &intermediate_der_certs);

  if (!ReadChainFromFile(target_path, &target_der_cert,
                         &intermediate_der_certs)) {
    std::cerr << "ERROR: Couldn't read certificate chain\n";
    return 1;
  }

  if (target_der_cert.der_cert.empty()) {
    std::cerr << "ERROR: no target cert\n";
    return 1;
  }

  // If --trust-last-cert was specified, move the final intermediate to the
  // roots list.
  if (command_line.HasSwitch("trust-last-cert")) {
    if (intermediate_der_certs.empty()) {
      std::cerr << "ERROR: no intermediate certificates\n";
      return 1;
    }

    root_der_certs.push_back(intermediate_der_certs.back());
    intermediate_der_certs.pop_back();
  }

  if (command_line.HasSwitch("trust-leaf-cert")) {
    bssl::CertificateTrust trust = bssl::CertificateTrust::ForTrustedLeaf();
    std::string trust_str = command_line.GetSwitchValueASCII("trust-leaf-cert");
    if (!trust_str.empty()) {
      std::optional<bssl::CertificateTrust> parsed_trust =
          bssl::CertificateTrust::FromDebugString(trust_str);
      if (!parsed_trust) {
        std::cerr << "ERROR: invalid leaf trust string " << trust_str << "\n";
        return 1;
      }
      trust = *parsed_trust;
    }
    der_certs_with_trust_settings.push_back({target_der_cert, trust});
  }

  // TODO(crbug.com/40888483): Maybe default to the trust setting that
  // would be used for locally added anchors on the current platform?
  bssl::CertificateTrust root_trust = bssl::CertificateTrust::ForTrustAnchor();

  if (command_line.HasSwitch("root-trust")) {
    std::string trust_str = command_line.GetSwitchValueASCII("root-trust");
    std::optional<bssl::CertificateTrust> parsed_trust =
        bssl::CertificateTrust::FromDebugString(trust_str);
    if (!parsed_trust) {
      std::cerr << "ERROR: invalid root trust string " << trust_str << "\n";
      return 1;
    }
    root_trust = *parsed_trust;
  }

  for (const auto& cert_input : root_der_certs) {
    der_certs_with_trust_settings.push_back({cert_input, root_trust});
  }

  PrintInputChain(target_der_cert, intermediate_der_certs);
  if (!der_certs_with_trust_settings.empty()) {
    PrintAdditionalRoots(der_certs_with_trust_settings);
  }

  // Create a network thread to be used for AIA fetches, and wait for a
  // CertNetFetcher to be constructed on that thread.
  base::Thread::Options options(base::MessagePumpType::IO, 0);
  base::Thread thread("network_thread");
  CHECK(thread.StartWithOptions(std::move(options)));
  // Owned by this thread, but initialized, used, and shutdown on the network
  // thread.
  std::unique_ptr<net::URLRequestContext> context;
  scoped_refptr<net::CertNetFetcherURLRequest> cert_net_fetcher;
  base::WaitableEvent initialization_complete_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SetUpOnNetworkThread, &context, &cert_net_fetcher,
                     &initialization_complete_event));
  initialization_complete_event.Wait();

  std::vector<std::unique_ptr<CertVerifyImpl>> impls;

  // Parse the ordered list of CertVerifyImpl passed via command line flags into
  // |impls|.
  std::string impls_str = command_line.GetSwitchValueASCII("impls");
  if (impls_str.empty()) {
    // Default value.
#if !(BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX) || \
      BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(CHROME_ROOT_STORE_ONLY))
    impls_str = "platform,";
#endif
    impls_str += "builtin,pathbuilder";
  }

  std::vector<std::string> impl_names = base::SplitString(
      impls_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (const std::string& impl_name : impl_names) {
    auto verify_impl = CreateCertVerifyImplFromName(impl_name, cert_net_fetcher,
                                                    crl_set, root_store_type);
    if (verify_impl)
      impls.push_back(std::move(verify_impl));
  }

  // Sequentially run the chain with each of the selected verifier
  // implementations.
  bool all_impls_success = true;

  for (size_t i = 0; i < impls.size(); ++i) {
    if (i != 0)
      std::cout << "\n";

    std::cout << impls[i]->GetName() << ":\n";
    if (!impls[i]->VerifyCert(target_der_cert, hostname, intermediate_der_certs,
                              der_certs_with_trust_settings, verify_time,
                              crl_set.get(), dump_prefix_path)) {
      all_impls_success = false;
    }
  }

  // Clean up on the network thread and stop it (which waits for the clean up
  // task to run).
  thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ShutdownOnNetworkThread, &context, &cert_net_fetcher));
  thread.Stop();

  return all_impls_success ? 0 : 1;
}
