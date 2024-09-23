// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "base/process/process_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/service_process_info.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/files/file_path.h"
#include "base/types/pass_key.h"
#endif  // BUILDFLAG(IS_WIN)

namespace base {
class Process;
}  // namespace base

namespace content {
// Passkeys for service process host Options.
class ServiceProcessHostGpuClient;
class ServiceProcessHostPreloadLibraries;

// Sandbox type for ServiceProcessHost::Launch<remote>() is found by
// template matching on |remote|. Consult security-dev@chromium.org and
// add a [ServiceSandbox=type] mojom attribute.
template <typename Interface>
inline sandbox::mojom::Sandbox GetServiceSandboxType() {
  using ProvidedSandboxType = decltype(Interface::kServiceSandbox);
  static_assert(
      std::is_same<ProvidedSandboxType, const sandbox::mojom::Sandbox>::value,
      "This interface does not declare a proper ServiceSandbox attribute. See "
      "//docs/mojo_and_services.md (Specifying a sandbox).");

  return Interface::kServiceSandbox;
}

// ServiceProcessHost is used to launch new service processes given basic
// parameters like sandbox type, as well as a primordial Mojo interface to drive
// the service's behavior. See |Launch()| methods below for more details.
//
// Typical usage might look something like:
//
//   constexpr auto kFooServiceIdleTimeout = base::Seconds(5);
//   auto foo_service = ServiceProcessHost::Launch<foo::mojom::FooService>(
//       ServiceProcessHost::Options()
//           .WithDisplayName(IDS_FOO_SERVICE_DISPLAY_NAME)
//           .Pass());
//   foo_service.set_idle_handler(
//       kFooServiceIdleTimeout,
//       base::BindRepeating(
//           /* Something to reset |foo_service|,  killing the process. */));
//   foo_service->DoWork();
//
class CONTENT_EXPORT ServiceProcessHost {
 public:
  struct CONTENT_EXPORT Options {
    Options();
    ~Options();

    Options(Options&&);

    // Specifies the display name of the service process. This should generally
    // be a human readable and meaningful application or service name and will
    // appear in places like the system task viewer.
    Options& WithDisplayName(const std::string& name);
    Options& WithDisplayName(const std::u16string& name);
    Options& WithDisplayName(int resource_id);

    // Specifies the site associated with the service process, only needed for
    // per-site service processes.
    Options& WithSite(const GURL& url);

    // Specifies additional flags to configure the launched process. See
    // ChildProcessHost for flag definitions.
    Options& WithChildFlags(int flags);

    // Specifies extra command line switches to append before launch.
    Options& WithExtraCommandLineSwitches(std::vector<std::string> switches);

    // Specifies a callback to be invoked with service process once it's
    // launched. Will be on UI thread.
    Options& WithProcessCallback(
        base::OnceCallback<void(const base::Process&)>);

#if BUILDFLAG(IS_WIN)
    // Specifies libraries to preload before the sandbox is locked down. Paths
    // should be absolute paths. Libraries will be preloaded before sandbox
    // lockdown. They should later be "loaded" in the utility process using the
    // same paths after lockdown.
    // Note that preloading does not occur with --no-sandbox - hence the need to
    // load in the utility with the full path - this api exists to make the
    // libraries available for later loading in the sandbox.
    Options& WithPreloadedLibraries(
        std::vector<base::FilePath> preload_libraries,
        base::PassKey<ServiceProcessHostPreloadLibraries> passkey);
#endif  // BUILDFLAG(IS_WIN)

    // Allows the viz.mojom.Gpu client to be bound via the process host on
    // platforms where that is supported. This option will be removed in future.
    // Prefer to avoid setting this option and instead bind the client directly
    // by passing a `pending_receiver<viz.mojom.Gpu>` to the service via mojo.
    Options& WithGpuClient(base::PassKey<ServiceProcessHostGpuClient> passkey);

    // Passes the contents of this Options object to a newly returned Options
    // value. This must be called when moving a built Options object into a call
    // to |Launch()|.
    Options Pass();

    std::u16string display_name;
    std::optional<GURL> site;
    std::optional<int> child_flags;
    std::vector<std::string> extra_switches;
    base::OnceCallback<void(const base::Process&)> process_callback;
#if BUILDFLAG(IS_WIN)
    std::vector<base::FilePath> preload_libraries;
#endif  // BUILDFLAG(IS_WIN)
    std::optional<bool> allow_gpu_client;
  };

  // An interface which can be implemented and registered/unregistered with
  // |Add/RemoveObserver()| below to watch for all service process creation and
  // and termination events globally. Methods are always called from the UI
  // UI thread.
  class CONTENT_EXPORT Observer : public base::CheckedObserver {
   public:
    ~Observer() override {}

    virtual void OnServiceProcessLaunched(const ServiceProcessInfo& info) {}
    virtual void OnServiceProcessTerminatedNormally(
        const ServiceProcessInfo& info) {}
    virtual void OnServiceProcessCrashed(const ServiceProcessInfo& info) {}
  };

  // Launches a new service process for asks it to bind the given interface
  // receiver. |Interface| must be a service interface known to utility process
  // code. See content/utility/services.cc and/or
  // ContentUtilityClient::Run{Main,IO}ThreadService() methods.
  //
  // The launched process will (disregarding crashes) stay alive until either
  // end of the |Interface| pipe is closed. Typically services are designed to
  // never close their end of this pipe, and it's up to the browser to
  // explicitly reset its corresponding Remote in order to induce service
  // process termination.
  //
  // The launched process will be sandboxed using the default utility process
  // sandbox unless a specialized GetServiceSandboxType<Interface> is available.
  // To add a new specialization, consult with security-dev@chromium.org.
  //
  // NOTE: The |Interface| type can be inferred from from the |receiver|
  // argument's type.
  //
  // May be called from any thread.
  template <typename Interface>
  static void Launch(mojo::PendingReceiver<Interface> receiver,
                     Options options = {}) {
    Launch(mojo::GenericPendingReceiver(std::move(receiver)),
           std::move(options), content::GetServiceSandboxType<Interface>());
  }

  // Same as above but creates a new |Interface| pipe on the caller's behalf and
  // returns its Remote endpoint.
  //
  // May be called from any thread.
  template <typename Interface>
  static mojo::Remote<Interface> Launch(Options options = {}) {
    mojo::Remote<Interface> remote;
    Launch(remote.BindNewPipeAndPassReceiver(), std::move(options),
           content::GetServiceSandboxType<Interface>());
    return remote;
  }

  // Yields information about currently active service processes. Must be called
  // from the UI Thread only.
  static std::vector<ServiceProcessInfo> GetRunningProcessInfo();

  // Registers a global observer of all service process lifetimes. Must be
  // removed before destruction. Must be called from the UI thread only.
  static void AddObserver(Observer* observer);

  // Removes a registered observer. This must be called some time before
  // |*observer| is destroyed and must be called from the UI thread only.
  static void RemoveObserver(Observer* observer);

 private:
  // Launches a new service process and asks it to bind a receiver for the
  // service interface endpoint carried by |receiver|, which should be connected
  // to a Remote of the same interface type.
  static void Launch(mojo::GenericPendingReceiver receiver,
                     Options options,
                     sandbox::mojom::Sandbox sandbox);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_H_
