// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_LINUX_H_
#define NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_LINUX_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "net/base/net_export.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
}  // namespace base

namespace net {

// Implementation of ProxyConfigService that retrieves the system proxy
// settings from environment variables, gconf, gsettings, or kioslaverc (KDE).
class NET_EXPORT_PRIVATE ProxyConfigServiceLinux : public ProxyConfigService {
 public:
  class Delegate;

  class SettingGetter {
   public:
    // Buffer size used in some implementations of this class when reading
    // files. Defined here so unit tests can construct worst-case inputs.
    static const size_t BUFFER_SIZE = 512;

    SettingGetter() = default;

    SettingGetter(const SettingGetter&) = delete;
    SettingGetter& operator=(const SettingGetter&) = delete;

    virtual ~SettingGetter() = default;

    // Initializes the class: obtains a gconf/gsettings client, or simulates
    // one, in the concrete implementations. Returns true on success. Must be
    // called before using other methods, and should be called on the thread
    // running the glib main loop.
    // This interface supports both GNOME and KDE implementations. In the
    // case of GNOME, the glib_task_runner will be used for interacting with
    // gconf/gsettings as those APIs have thread affinity. Whereas in the case
    // of KDE, its configuration files will be monitored at well-known locations
    // and glib_task_runner will not be used. Instead, blocking file I/O
    // operations will be posted directly using the task scheduler.
    virtual bool Init(const scoped_refptr<base::SingleThreadTaskRunner>&
                          glib_task_runner) = 0;

    // Releases the gconf/gsettings client, which clears cached directories and
    // stops notifications.
    virtual void ShutDown() = 0;

    // Requests notification of gconf/gsettings changes for proxy
    // settings. Returns true on success.
    virtual bool SetUpNotifications(Delegate* delegate) = 0;

    // Returns the message loop for the thread on which this object
    // handles notifications, and also on which it must be destroyed.
    // Returns NULL if it does not matter.
    virtual const scoped_refptr<base::SequencedTaskRunner>&
    GetNotificationTaskRunner() = 0;

    // These are all the values that can be fetched. We used to just use the
    // corresponding paths in gconf for these, but gconf is now obsolete and
    // in the future we'll be using mostly gsettings/kioslaverc so we
    // enumerate them instead to avoid unnecessary string operations.
    enum StringSetting {
      PROXY_MODE,
      PROXY_AUTOCONF_URL,
      PROXY_HTTP_HOST,
      PROXY_HTTPS_HOST,
      PROXY_FTP_HOST,
      PROXY_SOCKS_HOST,
    };
    enum BoolSetting {
      PROXY_USE_HTTP_PROXY,
      PROXY_USE_SAME_PROXY,
      PROXY_USE_AUTHENTICATION,
    };
    enum IntSetting {
      PROXY_HTTP_PORT,
      PROXY_HTTPS_PORT,
      PROXY_FTP_PORT,
      PROXY_SOCKS_PORT,
    };
    enum StringListSetting {
      PROXY_IGNORE_HOSTS,
    };

    // Given a PROXY_*_HOST value, return the corresponding PROXY_*_PORT value.
    static IntSetting HostSettingToPortSetting(StringSetting host) {
      switch (host) {
        case PROXY_HTTP_HOST:
          return PROXY_HTTP_PORT;
        case PROXY_HTTPS_HOST:
          return PROXY_HTTPS_PORT;
        case PROXY_FTP_HOST:
          return PROXY_FTP_PORT;
        case PROXY_SOCKS_HOST:
          return PROXY_SOCKS_PORT;
        default:
          NOTREACHED_IN_MIGRATION();
          return PROXY_HTTP_PORT;  // Placate compiler.
      }
    }

    // Gets a string type value from the data source and stores it in
    // |*result|. Returns false if the key is unset or on error. Must only be
    // called after a successful call to Init(), and not after a failed call
    // to SetUpNotifications() or after calling Release().
    virtual bool GetString(StringSetting key, std::string* result) = 0;
    // Same thing for a bool typed value.
    virtual bool GetBool(BoolSetting key, bool* result) = 0;
    // Same for an int typed value.
    virtual bool GetInt(IntSetting key, int* result) = 0;
    // And for a string list.
    virtual bool GetStringList(StringListSetting key,
                               std::vector<std::string>* result) = 0;

    // Returns true if the bypass list should be interpreted as a proxy
    // allow list rather than block list. (This is KDE-specific.)
    virtual bool BypassListIsReversed() = 0;

    // Returns true if bypass rules should evaluate using dumb string suffix
    // matches on the host. For instance when true, "notgoogle.com" will be
    // considered a match for "google.com", even though the bypass rule does not
    // include a wildcard, and the matched host is not a subdomain.
    virtual bool UseSuffixMatching() = 0;
  };

  // ProxyConfigServiceLinux is created on the glib thread, and
  // SetUpAndFetchInitialConfig() is immediately called to synchronously
  // fetch the original configuration and set up change notifications on
  // the ProxyConfigService's main SequencedTaskRunner, which is passed to its
  // constructor (Which may or may not run tasks on the glib thread).
  //
  // Past that point, it is accessed periodically through the
  // ProxyConfigService interface (GetLatestProxyConfig, AddObserver,
  // RemoveObserver) from the main TaskRunner.
  //
  // Setting change notification callbacks can occur at any time and are
  // run on either the glib thread (gconf/gsettings) or a separate file thread
  // (KDE). The new settings are fetched on that thread, and the resulting proxy
  // config is posted to the main TaskRunner through
  // Delegate::SetNewProxyConfig(). We then notify observers on that TaskRunner
  // of the configuration change.
  //
  // ProxyConfigServiceLinux is deleted from the main TaskRunner.
  //
  // The substance of the ProxyConfigServiceLinux implementation is
  // wrapped in the Delegate ref counted class. On deleting the
  // ProxyConfigServiceLinux, Delegate::OnDestroy() is posted to either
  // the glib thread (gconf/gsettings) or a file thread (KDE) where change
  // notifications will be safely stopped before releasing Delegate.

  class Delegate : public base::RefCountedThreadSafe<Delegate> {
   public:
    // Test code can set |setting_getter| and |traffic_annotation|. If left
    // unspecified, reasonable defaults will be used.
    Delegate(std::unique_ptr<base::Environment> env_var_getter,
             std::optional<std::unique_ptr<SettingGetter>> setting_getter,
             std::optional<NetworkTrafficAnnotationTag> traffic_annotation);

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Synchronously obtains the proxy configuration. If gconf,
    // gsettings, or kioslaverc are used, also enables notifications for
    // setting changes. gconf/gsettings must only be accessed from the
    // thread running the default glib main loop, and so this method
    // must be called from the glib thread. The message loop for the
    // ProxyConfigService's main SequencedTaskRunner is specified so that
    // notifications can post tasks to it (and for assertions).
    void SetUpAndFetchInitialConfig(
        const scoped_refptr<base::SingleThreadTaskRunner>& glib_task_runner,
        const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
        const NetworkTrafficAnnotationTag& traffic_annotation);

    // Handler for setting change notifications: fetches a new proxy
    // configuration from settings, and if this config is different
    // than what we had before, posts a task to have it stored in
    // cached_config_.
    // Left public for simplicity.
    void OnCheckProxyConfigSettings();

    // Called on the service's main TaskRunner.
    void AddObserver(Observer* observer);
    void RemoveObserver(Observer* observer);
    ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
        ProxyConfigWithAnnotation* config);

    // Posts a call to OnDestroy() to the glib or a file task runner,
    // depending on the setting getter in use. Called from
    // ProxyConfigServiceLinux's destructor.
    void PostDestroyTask();
    // Safely stops change notifications. Posted to either the glib thread or
    // sequenced task runner, depending on the setting getter in use.
    void OnDestroy();

   private:
    friend class base::RefCountedThreadSafe<Delegate>;

    ~Delegate();

    // Obtains an environment variable's value. Parses a proxy chain
    // specification from it and puts it in result. Returns true if the
    // requested variable is defined and the value valid.
    bool GetProxyFromEnvVarForScheme(std::string_view variable,
                                     ProxyServer::Scheme scheme,
                                     ProxyChain* result_chain);
    // As above but with scheme set to HTTP, for convenience.
    bool GetProxyFromEnvVar(std::string_view variable,
                            ProxyChain* result_chain);
    // Returns a proxy config based on the environment variables, or empty value
    // on failure.
    std::optional<ProxyConfigWithAnnotation> GetConfigFromEnv();

    // Obtains host and port config settings and parses a proxy server
    // specification from it and puts it in result. Returns true if the
    // requested variable is defined and the value valid.
    bool GetProxyFromSettings(SettingGetter::StringSetting host_key,
                              ProxyServer* result_server);
    // Returns a proxy config based on the settings, or empty value
    // on failure.
    std::optional<ProxyConfigWithAnnotation> GetConfigFromSettings();

    // This method is posted from the glib thread to the main TaskRunner to
    // carry the new config information.
    void SetNewProxyConfig(
        const std::optional<ProxyConfigWithAnnotation>& new_config);

    // This method is run on the getter's notification thread.
    void SetUpNotifications();

    std::unique_ptr<base::Environment> env_var_getter_;
    std::unique_ptr<SettingGetter> setting_getter_;

    // Cached proxy configuration, to be returned by
    // GetLatestProxyConfig. Initially populated from the glib thread, but
    // afterwards only accessed from the main TaskRunner.
    std::optional<ProxyConfigWithAnnotation> cached_config_;

    // A copy kept on the glib thread of the last seen proxy config, so as
    // to avoid posting a call to SetNewProxyConfig when we get a
    // notification but the config has not actually changed.
    std::optional<ProxyConfigWithAnnotation> reference_config_;

    // The task runner for the glib thread, aka main browser thread. This thread
    // is where we run the glib main loop (see
    // base/message_loop/message_pump_glib.h). It is the glib default loop in
    // the sense that it runs the glib default context: as in the context where
    // sources are added by g_timeout_add and g_idle_add, and returned by
    // g_main_context_default. gconf uses glib timeouts and idles and possibly
    // other callbacks that will all be dispatched on this thread. Since gconf
    // is not thread safe, any use of gconf must be done on the thread running
    // this loop.
    scoped_refptr<base::SingleThreadTaskRunner> glib_task_runner_;
    // Task runner for the main TaskRunner. GetLatestProxyConfig() is called
    // from the thread running this loop.
    scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

    base::ObserverList<Observer>::Unchecked observers_;

    MutableNetworkTrafficAnnotationTag traffic_annotation_;
  };

  // Thin wrapper shell around Delegate.

  // Usual constructor
  ProxyConfigServiceLinux();

  // For testing: take alternate setting and env var getter implementations.
  explicit ProxyConfigServiceLinux(
      std::unique_ptr<base::Environment> env_var_getter,
      const NetworkTrafficAnnotationTag& traffic_annotation);
  ProxyConfigServiceLinux(
      std::unique_ptr<base::Environment> env_var_getter,
      std::unique_ptr<SettingGetter> setting_getter,
      const NetworkTrafficAnnotationTag& traffic_annotation);

  ProxyConfigServiceLinux(const ProxyConfigServiceLinux&) = delete;
  ProxyConfigServiceLinux& operator=(const ProxyConfigServiceLinux&) = delete;

  ~ProxyConfigServiceLinux() override;

  void SetupAndFetchInitialConfig(
      const scoped_refptr<base::SingleThreadTaskRunner>& glib_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
      const NetworkTrafficAnnotationTag& traffic_annotation) {
    delegate_->SetUpAndFetchInitialConfig(glib_task_runner, main_task_runner,
                                          traffic_annotation);
  }
  void OnCheckProxyConfigSettings() {
    delegate_->OnCheckProxyConfigSettings();
  }

  // ProxyConfigService methods:
  // Called from main TaskRunner.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
      ProxyConfigWithAnnotation* config) override;

 private:
  scoped_refptr<Delegate> delegate_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_LINUX_H_
