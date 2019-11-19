// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "content/public/browser/sandbox_type.h"
#include "content/public/browser/service_process_info.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {

// ServiceProcessHost is used to launch new service processes given basic
// parameters like sandbox type, as well as a primordial Mojo interface to drive
// the service's behavior. See |Launch()| methods below for more details.
//
// Typical usage might look something like:
//
//   constexpr auto kFooServiceIdleTimeout = base::TimeDelta::FromSeconds(5);
//   auto foo_service = ServiceProcessHost::Launch<foo::mojom::FooService>(
//       ServiceProcessHost::Options()
//           .WithSandboxType(SANDBOX_TYPE_UTILITY)
//           .WithDisplayName(IDS_FOO_SERVICE_DISPLAY_NAME)
//           .Pass());
//   foo_service.set_idle_handler(
//       kFooServiceIdleTimeout,
//       base::BindRepeating(
//           /* Something to reset |foo_service|,  killing the process. */));
//   foo_service->DoSomeWork();
//
class CONTENT_EXPORT ServiceProcessHost {
 public:
  struct CONTENT_EXPORT Options {
    Options();
    ~Options();

    Options(Options&&);

    // Specifies the sandbox type with which to launch the service process.
    // Defaults to a generic, restrictive utility process sandbox.
    Options& WithSandboxType(SandboxType type);

    // Specifies the display name of the service process. This should generally
    // be a human readable and meaningful application or service name and will
    // appear in places like the system task viewer.
    Options& WithDisplayName(const std::string& name);
    Options& WithDisplayName(const base::string16& name);
    Options& WithDisplayName(int resource_id);

    // Specifies additional flags to configure the launched process. See
    // ChildProcessHost for flag definitions.
    Options& WithChildFlags(int flags);

    // Specifies extra command line switches to append before launch.
    Options& WithExtraCommandLineSwitches(std::vector<std::string> switches);

    // Passes the contents of this Options object to a newly returned Options
    // value. This must be called when moving a built Options object into a call
    // to |Launch()|.
    Options Pass();

    SandboxType sandbox_type = service_manager::SANDBOX_TYPE_UTILITY;
    base::string16 display_name;
    base::Optional<int> child_flags;
    std::vector<std::string> extra_switches;
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
  // NOTE: The |Interface| type can be inferred from from the |receiver|
  // argument's type.
  //
  // May be called from any thread.
  template <typename Interface>
  static void Launch(mojo::PendingReceiver<Interface> receiver,
                     Options options = {}) {
    Launch(mojo::GenericPendingReceiver(std::move(receiver)),
           std::move(options));
  }

  // Same as above but creates a new |Interface| pipe on the caller's behalf and
  // returns its Remote endpoint.
  //
  // May be called from any thread.
  template <typename Interface>
  static mojo::Remote<Interface> Launch(Options options = {}) {
    mojo::Remote<Interface> remote;
    Launch(remote.BindNewPipeAndPassReceiver(), std::move(options));
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
  static void Launch(mojo::GenericPendingReceiver receiver, Options options);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_H_
