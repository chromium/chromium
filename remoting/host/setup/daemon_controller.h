// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class AutoThread;
class AutoThreadTaskRunner;

class DaemonController : public base::RefCountedThreadSafe<DaemonController> {
 public:
  // These enumeration values are duplicated in host_controller.js except that
  // NOT_INSTALLED is missing here. DaemonController runs in either the remoting
  // host or the native messaging host which are only installed as part of the
  // host package so the host must have already been installed.
  enum State {
    // Placeholder state for platforms on which the daemon process is not
    // implemented. The web-app will not show the corresponding UI. This value
    // will eventually be deprecated or removed.
    STATE_NOT_IMPLEMENTED = 0,
    // The daemon is installed but not running. Call Start to start it.
    STATE_STOPPED = 2,
    // The daemon process is starting.
    STATE_STARTING = 3,
    // The daemon process is running. Call Start again to change the PIN or
    // Stop to stop it.
    STATE_STARTED = 4,
    // The daemon process is stopping.
    STATE_STOPPING = 5,
    // The state cannot be determined.
    STATE_UNKNOWN = 6
  };

  // Enum used for completion callback.
  enum AsyncResult {
    RESULT_OK = 0,

    // The operation has FAILED.
    RESULT_FAILED = 1,

    // User has cancelled the action (e.g. rejected UAC prompt).
    // TODO(sergeyu): Current implementations don't return this value.
    RESULT_CANCELLED = 2,

    // TODO(sergeyu): Add more error codes when we know how to handle
    // them in the webapp.
  };

  // Callback type for GetConfig(). If the host is configured then a dictionary
  // is returned containing host_id and service_account, with security-sensitive
  // fields filtered out. An empty dictionary is returned if the host is not
  // configured, and nullptr if the configuration is corrupt or cannot be read.
  typedef base::OnceCallback<void(std::optional<base::Value::Dict> config)>
      GetConfigCallback;

  // Callback used for asynchronous operations, e.g. when
  // starting/stopping the service.
  typedef base::OnceCallback<void(AsyncResult result)> CompletionCallback;

  // Callback used to notify a Boolean result.
  typedef base::OnceCallback<void(bool)> BoolCallback;

  struct UsageStatsConsent {
    // Indicates whether crash dump reporting is supported by the host.
    bool supported;

    // Indicates if crash dump reporting is allowed by the user.
    bool allowed;

    // Carries information whether the crash dump reporting is controlled by
    // policy.
    bool set_by_policy;
  };

  // Callback type for GetUsageStatsConsent().
  typedef base::OnceCallback<void(const UsageStatsConsent&)>
      GetUsageStatsConsentCallback;

  // Interface representing the platform-spacific back-end. Most of its methods
  // are blocking and should be called on a background thread. There are two
  // exceptions:
  //   - GetState() is synchronous and called on the UI thread. It should avoid
  //         accessing any data members of the implementation.
  //   - SetConfigAndStart(), UpdateConfig() and Stop() indicate completion via
  //         a callback. There methods can be long running and should be caled
  //         on a background thread.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Return the "installed/running" state of the daemon process. This method
    // should avoid accessing any data members of the implementation.
    virtual State GetState() = 0;

    // Queries current host configuration. Any values that might be security
    // sensitive have been filtered out.
    virtual std::optional<base::Value::Dict> GetConfig() = 0;

    // Checks to verify that the required OS permissions have been granted to
    // the host process, querying the user if necessary. Notifies the callback
    // when permission status is established, passing true iff all required
    // permissions have been granted.
    virtual void CheckPermission(bool it2me, BoolCallback callback) = 0;

    // Starts the daemon process. This may require that the daemon be
    // downloaded and installed. |done| is invoked on the calling thread when
    // the operation is completed.
    virtual void SetConfigAndStart(base::Value::Dict config,
                                   bool consent,
                                   CompletionCallback done) = 0;

    // Updates current host configuration with the values specified in
    // |config|. Any value in the existing configuration that isn't specified in
    // |config| is preserved. |config| must not contain host_id, xmpp_login, or
    // service_account values, because implementations of this method cannot
    // change them. |done| is invoked on the calling thread when the operation
    // is completed.
    virtual void UpdateConfig(base::Value::Dict config,
                              CompletionCallback done) = 0;

    // Stops the daemon process. |done| is invoked on the calling thread when
    // the operation is completed.
    virtual void Stop(CompletionCallback done) = 0;

    // Get the user's consent to crash reporting.
    virtual UsageStatsConsent GetUsageStatsConsent() = 0;
  };

  static scoped_refptr<DaemonController> Create();

  explicit DaemonController(std::unique_ptr<Delegate> delegate);

  DaemonController(const DaemonController&) = delete;
  DaemonController& operator=(const DaemonController&) = delete;

  // Return the "installed/running" state of the daemon process.
  //
  // TODO(sergeyu): This method is called synchronously from the
  // webapp. In most cases it requires IO operations, so it may block
  // the user interface. Replace it with asynchronous notifications,
  // e.g. with StartStateNotifications()/StopStateNotifications() methods.
  State GetState();

  // Queries current host configuration. The |done| is called
  // after the configuration is read, and any values that might be security
  // sensitive have been filtered out.
  void GetConfig(GetConfigCallback done);

  // Checks to see if the required OS permissions have been granted. This may
  // show a dialog to the user requesting the permissions.
  // Notifies the callback when permission status is established, passing true
  // iff all required permissions have been granted.
  void CheckPermission(bool it2me, BoolCallback callback);

  // Start the daemon process. This may require that the daemon be
  // downloaded and installed. |done| is called when the
  // operation is finished or fails.
  //
  // TODO(sergeyu): This method writes config and starts the host -
  // these two steps are merged for simplicity. Consider splitting it
  // into SetConfig() and Start() once we have basic host setup flow
  // working.
  void SetConfigAndStart(base::Value::Dict config,
                         bool consent,
                         CompletionCallback done);

  // Updates current host configuration with the values specified in
  // |config|. Changes must take effect before the call completes.
  // Any value in the existing configuration that isn't specified in |config|
  // is preserved. |config| must not contain host_id, xmpp_login, or
  // service_account values, because implementations of this method cannot
  // change them.
  void UpdateConfig(base::Value::Dict config, CompletionCallback done);

  // Stop the daemon process. It is permitted to call Stop while the daemon
  // process is being installed, in which case the installation should be
  // aborted if possible; if not then it is sufficient to ensure that the
  // daemon process is not started automatically upon successful installation.
  // As with Start, Stop may return before the operation is complete--poll
  // GetState until the state is STATE_STOPPED.
  void Stop(CompletionCallback done);

  // Get the user's consent to crash reporting.
  void GetUsageStatsConsent(GetUsageStatsConsentCallback done);

 private:
  friend class base::RefCountedThreadSafe<DaemonController>;
  virtual ~DaemonController();

  // Blocking helper methods used to call the delegate.
  void DoGetConfig(GetConfigCallback done);
  void DoSetConfigAndStart(base::Value::Dict config,
                           bool consent,
                           CompletionCallback done);
  void DoUpdateConfig(base::Value::Dict config, CompletionCallback done);
  void DoStop(CompletionCallback done);
  void DoGetUsageStatsConsent(GetUsageStatsConsentCallback done);

  // "Trampoline" callbacks that schedule the next pending request and then
  // invoke the original caller-supplied callback.
  void InvokeCompletionCallbackAndScheduleNext(CompletionCallback done,
                                               AsyncResult result);
  void InvokeConfigCallbackAndScheduleNext(
      GetConfigCallback done,
      std::optional<base::Value::Dict> config);
  void InvokeConsentCallbackAndScheduleNext(GetUsageStatsConsentCallback done,
                                            const UsageStatsConsent& consent);

  // Queue management methods.
  void OnServicingDone();
  void ServiceOrQueueRequest(base::OnceClosure request);
  void ServiceNextRequest();

  // Task runner on which all public methods of this class should be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner used to run blocking calls to the delegate. A single thread
  // task runner is used to guarantee that one method of the delegate is
  // called at a time.
  scoped_refptr<AutoThreadTaskRunner> delegate_task_runner_;

  std::unique_ptr<AutoThread> delegate_thread_;

  std::unique_ptr<Delegate> delegate_;

  bool servicing_request_ = false;
  base::queue<base::OnceClosure> pending_requests_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_H_
