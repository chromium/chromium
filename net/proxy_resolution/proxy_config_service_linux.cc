// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config_service_linux.h"

#include <errno.h>
#include <limits.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/nix/xdg_util.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/timer.h"
#include "net/base/proxy_server.h"

#if defined(USE_GIO)
#include <gio/gio.h>
#endif  // defined(USE_GIO)

namespace net {

namespace {

// Given a proxy hostname from a setting, returns that hostname with
// an appropriate proxy server scheme prefix.
// scheme indicates the desired proxy scheme: usually http, with
// socks 4 or 5 as special cases.
// TODO(arindam): Remove URI string manipulation by using MapUrlSchemeToProxy.
std::string FixupProxyHostScheme(ProxyServer::Scheme scheme,
                                 std::string host) {
  if (scheme == ProxyServer::SCHEME_SOCKS5 &&
      base::StartsWith(host, "socks4://",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    // We default to socks 5, but if the user specifically set it to
    // socks4://, then use that.
    scheme = ProxyServer::SCHEME_SOCKS4;
  }
  // Strip the scheme if any.
  std::string::size_type colon = host.find("://");
  if (colon != std::string::npos)
    host = host.substr(colon + 3);
  // If a username and perhaps password are specified, give a warning.
  std::string::size_type at_sign = host.find("@");
  // Should this be supported?
  if (at_sign != std::string::npos) {
    // ProxyConfig does not support authentication parameters, but Chrome
    // will prompt for the password later. Disregard the
    // authentication parameters and continue with this hostname.
    LOG(WARNING) << "Proxy authentication parameters ignored, see bug 16709";
    host = host.substr(at_sign + 1);
  }
  // If this is a socks proxy, prepend a scheme so as to tell
  // ProxyServer. This also allows ProxyServer to choose the right
  // default port.
  if (scheme == ProxyServer::SCHEME_SOCKS4)
    host = "socks4://" + host;
  else if (scheme == ProxyServer::SCHEME_SOCKS5)
    host = "socks5://" + host;
  // If there is a trailing slash, remove it so |host| will parse correctly
  // even if it includes a port number (since the slash is not numeric).
  if (!host.empty() && host.back() == '/')
    host.resize(host.length() - 1);
  return host;
}

ProxyConfigWithAnnotation GetConfigOrDirect(
    const base::Optional<ProxyConfigWithAnnotation>& optional_config) {
  if (optional_config)
    return optional_config.value();

  ProxyConfigWithAnnotation config = ProxyConfigWithAnnotation::CreateDirect();
  return config;
}

}  // namespace

ProxyConfigServiceLinux::Delegate::~Delegate() = default;

bool ProxyConfigServiceLinux::Delegate::GetProxyFromEnvVarForScheme(
    base::StringPiece variable,
    ProxyServer::Scheme scheme,
    ProxyServer* result_server) {
  std::string env_value;
  if (!env_var_getter_->GetVar(variable, &env_value))
    return false;

  if (env_value.empty())
    return false;

  env_value = FixupProxyHostScheme(scheme, env_value);
  ProxyServer proxy_server =
      ProxyServer::FromURI(env_value, ProxyServer::SCHEME_HTTP);
  if (proxy_server.is_valid() && !proxy_server.is_direct()) {
    *result_server = proxy_server;
    return true;
  }
  LOG(ERROR) << "Failed to parse environment variable " << variable;
  return false;
}

bool ProxyConfigServiceLinux::Delegate::GetProxyFromEnvVar(
    base::StringPiece variable,
    ProxyServer* result_server) {
  return GetProxyFromEnvVarForScheme(variable, ProxyServer::SCHEME_HTTP,
                                     result_server);
}

base::Optional<ProxyConfigWithAnnotation>
ProxyConfigServiceLinux::Delegate::GetConfigFromEnv() {
  ProxyConfig config;

  // Check for automatic configuration first, in
  // "auto_proxy". Possibly only the "environment_proxy" firefox
  // extension has ever used this, but it still sounds like a good
  // idea.
  std::string auto_proxy;
  if (env_var_getter_->GetVar("auto_proxy", &auto_proxy)) {
    if (auto_proxy.empty()) {
      // Defined and empty => autodetect
      config.set_auto_detect(true);
    } else {
      // specified autoconfig URL
      config.set_pac_url(GURL(auto_proxy));
    }
    return ProxyConfigWithAnnotation(
        config, NetworkTrafficAnnotationTag(traffic_annotation_));
  }
  // "all_proxy" is a shortcut to avoid defining {http,https,ftp}_proxy.
  ProxyServer proxy_server;
  if (GetProxyFromEnvVar("all_proxy", &proxy_server)) {
    config.proxy_rules().type = ProxyConfig::ProxyRules::Type::PROXY_LIST;
    config.proxy_rules().single_proxies.SetSingleProxyServer(proxy_server);
  } else {
    bool have_http = GetProxyFromEnvVar("http_proxy", &proxy_server);
    if (have_http)
      config.proxy_rules().proxies_for_http.SetSingleProxyServer(proxy_server);
    // It would be tempting to let http_proxy apply for all protocols
    // if https_proxy and ftp_proxy are not defined. Googling turns up
    // several documents that mention only http_proxy. But then the
    // user really might not want to proxy https. And it doesn't seem
    // like other apps do this. So we will refrain.
    bool have_https = GetProxyFromEnvVar("https_proxy", &proxy_server);
    if (have_https)
      config.proxy_rules().proxies_for_https.SetSingleProxyServer(proxy_server);
    bool have_ftp = GetProxyFromEnvVar("ftp_proxy", &proxy_server);
    if (have_ftp)
      config.proxy_rules().proxies_for_ftp.SetSingleProxyServer(proxy_server);
    if (have_http || have_https || have_ftp) {
      // mustn't change type unless some rules are actually set.
      config.proxy_rules().type =
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
    }
  }
  if (config.proxy_rules().empty()) {
    // If the above were not defined, try for socks.
    // For environment variables, we default to version 5, per the gnome
    // documentation: http://library.gnome.org/devel/gnet/stable/gnet-socks.html
    ProxyServer::Scheme scheme = ProxyServer::SCHEME_SOCKS5;
    std::string env_version;
    if (env_var_getter_->GetVar("SOCKS_VERSION", &env_version)
        && env_version == "4")
      scheme = ProxyServer::SCHEME_SOCKS4;
    if (GetProxyFromEnvVarForScheme("SOCKS_SERVER", scheme, &proxy_server)) {
      config.proxy_rules().type = ProxyConfig::ProxyRules::Type::PROXY_LIST;
      config.proxy_rules().single_proxies.SetSingleProxyServer(proxy_server);
    }
  }
  // Look for the proxy bypass list.
  std::string no_proxy;
  env_var_getter_->GetVar("no_proxy", &no_proxy);
  if (config.proxy_rules().empty()) {
    // Having only "no_proxy" set, presumably to "*", makes it
    // explicit that env vars do specify a configuration: having no
    // rules specified only means the user explicitly asks for direct
    // connections.
    return !no_proxy.empty()
               ? ProxyConfigWithAnnotation(
                     config, NetworkTrafficAnnotationTag(traffic_annotation_))
               : base::Optional<ProxyConfigWithAnnotation>();
  }
  // Note that this uses "suffix" matching. So a bypass of "google.com"
  // is understood to mean a bypass of "*google.com".
  config.proxy_rules().bypass_rules.ParseFromString(
      no_proxy, ProxyBypassRules::ParseFormat::kHostnameSuffixMatching);
  return ProxyConfigWithAnnotation(
      config, NetworkTrafficAnnotationTag(traffic_annotation_));
}

namespace {

const int kDebounceTimeoutMilliseconds = 250;

#if defined(USE_GIO)
const char kProxyGSettingsSchema[] = "org.gnome.system.proxy";

// This setting getter uses gsettings, as used in most GNOME 3 desktops.
class SettingGetterImplGSettings
    : public ProxyConfigServiceLinux::SettingGetter {
 public:
  SettingGetterImplGSettings()
      : client_(nullptr),
        http_client_(nullptr),
        https_client_(nullptr),
        ftp_client_(nullptr),
        socks_client_(nullptr),
        notify_delegate_(nullptr),
        debounce_timer_(new base::OneShotTimer()) {}

  ~SettingGetterImplGSettings() override {
    // client_ should have been released before now, from
    // Delegate::OnDestroy(), while running on the UI thread. However
    // on exiting the process, it may happen that
    // Delegate::OnDestroy() task is left pending on the glib loop
    // after the loop was quit, and pending tasks may then be deleted
    // without being run.
    if (client_) {
      // gsettings client was not cleaned up.
      if (task_runner_->RunsTasksInCurrentSequence()) {
        // We are on the UI thread so we can clean it safely.
        VLOG(1) << "~SettingGetterImplGSettings: releasing gsettings client";
        ShutDown();
      } else {
        LOG(WARNING) << "~SettingGetterImplGSettings: leaking gsettings client";
        client_ = nullptr;
      }
    }
    DCHECK(!client_);
  }

  // CheckVersion() must be called *before* Init()!
  bool CheckVersion(base::Environment* env);

  bool Init(const scoped_refptr<base::SingleThreadTaskRunner>& glib_task_runner)
      override {
    DCHECK(glib_task_runner->RunsTasksInCurrentSequence());
    DCHECK(!client_);
    DCHECK(!task_runner_.get());

    if (!g_settings_schema_source_lookup(g_settings_schema_source_get_default(),
                                         kProxyGSettingsSchema, FALSE) ||
        !(client_ = g_settings_new(kProxyGSettingsSchema))) {
      // It's not clear whether/when this can return NULL.
      LOG(ERROR) << "Unable to create a gsettings client";
      return false;
    }
    task_runner_ = glib_task_runner;
    // We assume these all work if the above call worked.
    http_client_ = g_settings_get_child(client_, "http");
    https_client_ = g_settings_get_child(client_, "https");
    ftp_client_ = g_settings_get_child(client_, "ftp");
    socks_client_ = g_settings_get_child(client_, "socks");
    DCHECK(http_client_ && https_client_ && ftp_client_ && socks_client_);
    return true;
  }

  void ShutDown() override {
    if (client_) {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      // This also disables gsettings notifications.
      g_object_unref(socks_client_);
      g_object_unref(ftp_client_);
      g_object_unref(https_client_);
      g_object_unref(http_client_);
      g_object_unref(client_);
      // We only need to null client_ because it's the only one that we check.
      client_ = nullptr;
      task_runner_ = nullptr;
    }
    debounce_timer_.reset();
  }

  bool SetUpNotifications(
      ProxyConfigServiceLinux::Delegate* delegate) override {
    DCHECK(client_);
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    notify_delegate_ = delegate;
    // We could watch for the change-event signal instead of changed, but
    // since we have to watch more than one object, we'd still have to
    // debounce change notifications. This is conceptually simpler.
    g_signal_connect(G_OBJECT(client_), "changed",
                     G_CALLBACK(OnGSettingsChangeNotification), this);
    g_signal_connect(G_OBJECT(http_client_), "changed",
                     G_CALLBACK(OnGSettingsChangeNotification), this);
    g_signal_connect(G_OBJECT(https_client_), "changed",
                     G_CALLBACK(OnGSettingsChangeNotification), this);
    g_signal_connect(G_OBJECT(ftp_client_), "changed",
                     G_CALLBACK(OnGSettingsChangeNotification), this);
    g_signal_connect(G_OBJECT(socks_client_), "changed",
                     G_CALLBACK(OnGSettingsChangeNotification), this);
    // Simulate a change to avoid possibly losing updates before this point.
    OnChangeNotification();
    return true;
  }

  const scoped_refptr<base::SequencedTaskRunner>& GetNotificationTaskRunner()
      override {
    return task_runner_;
  }

  bool GetString(StringSetting key, std::string* result) override {
    DCHECK(client_);
    switch (key) {
      case PROXY_MODE:
        return GetStringByPath(client_, "mode", result);
      case PROXY_AUTOCONF_URL:
        return GetStringByPath(client_, "autoconfig-url", result);
      case PROXY_HTTP_HOST:
        return GetStringByPath(http_client_, "host", result);
      case PROXY_HTTPS_HOST:
        return GetStringByPath(https_client_, "host", result);
      case PROXY_FTP_HOST:
        return GetStringByPath(ftp_client_, "host", result);
      case PROXY_SOCKS_HOST:
        return GetStringByPath(socks_client_, "host", result);
    }
    return false;  // Placate compiler.
  }
  bool GetBool(BoolSetting key, bool* result) override {
    DCHECK(client_);
    switch (key) {
      case PROXY_USE_HTTP_PROXY:
        // Although there is an "enabled" boolean in http_client_, it is not set
        // to true by the proxy config utility. We ignore it and return false.
        return false;
      case PROXY_USE_SAME_PROXY:
        // Similarly, although there is a "use-same-proxy" boolean in client_,
        // it is never set to false by the proxy config utility. We ignore it.
        return false;
      case PROXY_USE_AUTHENTICATION:
        // There is also no way to set this in the proxy config utility, but it
        // doesn't hurt us to get the actual setting (unlike the two above).
        return GetBoolByPath(http_client_, "use-authentication", result);
    }
    return false;  // Placate compiler.
  }
  bool GetInt(IntSetting key, int* result) override {
    DCHECK(client_);
    switch (key) {
      case PROXY_HTTP_PORT:
        return GetIntByPath(http_client_, "port", result);
      case PROXY_HTTPS_PORT:
        return GetIntByPath(https_client_, "port", result);
      case PROXY_FTP_PORT:
        return GetIntByPath(ftp_client_, "port", result);
      case PROXY_SOCKS_PORT:
        return GetIntByPath(socks_client_, "port", result);
    }
    return false;  // Placate compiler.
  }
  bool GetStringList(StringListSetting key,
                     std::vector<std::string>* result) override {
    DCHECK(client_);
    switch (key) {
      case PROXY_IGNORE_HOSTS:
        return GetStringListByPath(client_, "ignore-hosts", result);
    }
    return false;  // Placate compiler.
  }

  bool BypassListIsReversed() override {
    // This is a KDE-specific setting.
    return false;
  }

  ProxyBypassRules::ParseFormat GetBypassListFormat() override {
    return ProxyBypassRules::ParseFormat::kDefault;
  }

 private:
  bool GetStringByPath(GSettings* client,
                       base::StringPiece key,
                       std::string* result) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    gchar* value = g_settings_get_string(client, key.data());
    if (!value)
      return false;
    *result = value;
    g_free(value);
    return true;
  }
  bool GetBoolByPath(GSettings* client, base::StringPiece key, bool* result) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    *result = static_cast<bool>(g_settings_get_boolean(client, key.data()));
    return true;
  }
  bool GetIntByPath(GSettings* client, base::StringPiece key, int* result) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    *result = g_settings_get_int(client, key.data());
    return true;
  }
  bool GetStringListByPath(GSettings* client,
                           base::StringPiece key,
                           std::vector<std::string>* result) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    gchar** list = g_settings_get_strv(client, key.data());
    if (!list)
      return false;
    for (size_t i = 0; list[i]; ++i) {
      result->push_back(static_cast<char*>(list[i]));
      g_free(list[i]);
    }
    g_free(list);
    return true;
  }

  // This is the callback from the debounce timer.
  void OnDebouncedNotification() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    CHECK(notify_delegate_);
    // Forward to a method on the proxy config service delegate object.
    notify_delegate_->OnCheckProxyConfigSettings();
  }

  void OnChangeNotification() {
    // We don't use Reset() because the timer may not yet be running.
    // (In that case Stop() is a no-op.)
    debounce_timer_->Stop();
    debounce_timer_->Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kDebounceTimeoutMilliseconds),
        this, &SettingGetterImplGSettings::OnDebouncedNotification);
  }

  // gsettings notification callback, dispatched on the default glib main loop.
  static void OnGSettingsChangeNotification(GSettings* client, gchar* key,
                                            gpointer user_data) {
    VLOG(1) << "gsettings change notification for key " << key;
    // We don't track which key has changed, just that something did change.
    SettingGetterImplGSettings* setting_getter =
        reinterpret_cast<SettingGetterImplGSettings*>(user_data);
    setting_getter->OnChangeNotification();
  }

  GSettings* client_;
  GSettings* http_client_;
  GSettings* https_client_;
  GSettings* ftp_client_;
  GSettings* socks_client_;
  ProxyConfigServiceLinux::Delegate* notify_delegate_;
  std::unique_ptr<base::OneShotTimer> debounce_timer_;

  // Task runner for the thread that we make gsettings calls on. It should
  // be the UI thread and all our methods should be called on this
  // thread. Only for assertions.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SettingGetterImplGSettings);
};

bool SettingGetterImplGSettings::CheckVersion(
    base::Environment* env) {
  // CheckVersion() must be called *before* Init()!
  DCHECK(!client_);

  GSettings* client = nullptr;
  if (g_settings_schema_source_lookup(g_settings_schema_source_get_default(),
                                      kProxyGSettingsSchema, FALSE)) {
    client = g_settings_new(kProxyGSettingsSchema);
  }
  if (!client) {
    VLOG(1) << "Cannot create gsettings client.";
    return false;
  }
  g_object_unref(client);

  VLOG(1) << "All gsettings tests OK. Will get proxy config from gsettings.";
  return true;
}
#endif  // defined(USE_GIO)

// Converts |value| from a decimal string to an int. If there was a failure
// parsing, returns |default_value|.
int StringToIntOrDefault(base::StringPiece value, int default_value) {
  int result;
  if (base::StringToInt(value, &result))
    return result;
  return default_value;
}

// This is the KDE version that reads kioslaverc and simulates gsettings.
// Doing this allows the main Delegate code, as well as the unit tests
// for it, to stay the same - and the settings map fairly well besides.
class SettingGetterImplKDE : public ProxyConfigServiceLinux::SettingGetter {
 public:
  explicit SettingGetterImplKDE(base::Environment* env_var_getter)
      : inotify_fd_(-1),
        notify_delegate_(nullptr),
        debounce_timer_(new base::OneShotTimer()),
        indirect_manual_(false),
        auto_no_pac_(false),
        reversed_bypass_list_(false),
        env_var_getter_(env_var_getter),
        file_task_runner_(nullptr) {
    // This has to be called on the UI thread (http://crbug.com/69057).
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    // Derive the location of the kde config dir from the environment.
    std::string home;
    if (env_var_getter->GetVar("KDEHOME", &home) && !home.empty()) {
      // $KDEHOME is set. Use it unconditionally.
      kde_config_dir_ = KDEHomeToConfigPath(base::FilePath(home));
    } else {
      // $KDEHOME is unset. Try to figure out what to use. This seems to be
      // the common case on most distributions.
      if (!env_var_getter->GetVar(base::env_vars::kHome, &home))
        // User has no $HOME? Give up. Later we'll report the failure.
        return;
      if (base::nix::GetDesktopEnvironment(env_var_getter) ==
          base::nix::DESKTOP_ENVIRONMENT_KDE3) {
        // KDE3 always uses .kde for its configuration.
        base::FilePath kde_path = base::FilePath(home).Append(".kde");
        kde_config_dir_ = KDEHomeToConfigPath(kde_path);
      } else if (base::nix::GetDesktopEnvironment(env_var_getter) ==
                 base::nix::DESKTOP_ENVIRONMENT_KDE4) {
        // Some distributions patch KDE4 to use .kde4 instead of .kde, so that
        // both can be installed side-by-side. Sadly they don't all do this, and
        // they don't always do this: some distributions have started switching
        // back as well. So if there is a .kde4 directory, check the timestamps
        // of the config directories within and use the newest one.
        // Note that we should currently be running in the UI thread, because in
        // the gsettings version, that is the only thread that can access the
        // proxy settings (a gsettings restriction). As noted below, the initial
        // read of the proxy settings will be done in this thread anyway, so we
        // check for .kde4 here in this thread as well.
        base::FilePath kde3_path = base::FilePath(home).Append(".kde");
        base::FilePath kde3_config = KDEHomeToConfigPath(kde3_path);
        base::FilePath kde4_path = base::FilePath(home).Append(".kde4");
        base::FilePath kde4_config = KDEHomeToConfigPath(kde4_path);
        bool use_kde4 = false;
        if (base::DirectoryExists(kde4_path)) {
          base::File::Info kde3_info;
          base::File::Info kde4_info;
          if (base::GetFileInfo(kde4_config, &kde4_info)) {
            if (base::GetFileInfo(kde3_config, &kde3_info)) {
              use_kde4 = kde4_info.last_modified >= kde3_info.last_modified;
            } else {
              use_kde4 = true;
            }
          }
        }
        if (use_kde4) {
          kde_config_dir_ = KDEHomeToConfigPath(kde4_path);
        } else {
          kde_config_dir_ = KDEHomeToConfigPath(kde3_path);
        }
      } else {
        // KDE 5 migrated to ~/.config for storing kioslaverc.
        kde_config_dir_ = base::FilePath(home).Append(".config");
      }
    }
  }

  ~SettingGetterImplKDE() override {
    // inotify_fd_ should have been closed before now, from
    // Delegate::OnDestroy(), while running on the file thread. However
    // on exiting the process, it may happen that Delegate::OnDestroy()
    // task is left pending on the file loop after the loop was quit,
    // and pending tasks may then be deleted without being run.
    // Here in the KDE version, we can safely close the file descriptor
    // anyway. (Not that it really matters; the process is exiting.)
    if (inotify_fd_ >= 0)
      ShutDown();
    DCHECK_LT(inotify_fd_, 0);
  }

  bool Init(const scoped_refptr<base::SingleThreadTaskRunner>& glib_task_runner)
      override {
    // This has to be called on the UI thread (http://crbug.com/69057).
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    DCHECK_LT(inotify_fd_, 0);
    inotify_fd_ = inotify_init();
    if (inotify_fd_ < 0) {
      PLOG(ERROR) << "inotify_init failed";
      return false;
    }
    if (!base::SetNonBlocking(inotify_fd_)) {
      PLOG(ERROR) << "base::SetNonBlocking failed";
      close(inotify_fd_);
      inotify_fd_ = -1;
      return false;
    }

    constexpr base::TaskTraits kTraits = {
        base::ThreadPool(), base::TaskPriority::USER_VISIBLE, base::MayBlock()};
    file_task_runner_ = base::CreateSequencedTaskRunner(kTraits);

    // The initial read is done on the current thread, not
    // |file_task_runner_|, since we will need to have it for
    // SetUpAndFetchInitialConfig().
    UpdateCachedSettings();
    return true;
  }

  void ShutDown() override {
    if (inotify_fd_ >= 0) {
      ResetCachedSettings();
      inotify_watcher_.reset();
      close(inotify_fd_);
      inotify_fd_ = -1;
    }
    debounce_timer_.reset();
  }

  bool SetUpNotifications(
      ProxyConfigServiceLinux::Delegate* delegate) override {
    DCHECK_GE(inotify_fd_, 0);
    DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
    // We can't just watch the kioslaverc file directly, since KDE will write
    // a new copy of it and then rename it whenever settings are changed and
    // inotify watches inodes (so we'll be watching the old deleted file after
    // the first change, and it will never change again). So, we watch the
    // directory instead. We then act only on changes to the kioslaverc entry.
    // TODO(eroman): What if the file is deleted? (handle with IN_DELETE).
    if (inotify_add_watch(inotify_fd_, kde_config_dir_.value().c_str(),
                          IN_MODIFY | IN_MOVED_TO) < 0) {
      return false;
    }
    notify_delegate_ = delegate;
    inotify_watcher_ = base::FileDescriptorWatcher::WatchReadable(
        inotify_fd_, base::Bind(&SettingGetterImplKDE::OnChangeNotification,
                                base::Unretained(this)));
    // Simulate a change to avoid possibly losing updates before this point.
    OnChangeNotification();
    return true;
  }

  const scoped_refptr<base::SequencedTaskRunner>& GetNotificationTaskRunner()
      override {
    return file_task_runner_;
  }

  bool GetString(StringSetting key, std::string* result) override {
    auto it = string_table_.find(key);
    if (it == string_table_.end())
      return false;
    *result = it->second;
    return true;
  }
  bool GetBool(BoolSetting key, bool* result) override {
    // We don't ever have any booleans.
    return false;
  }
  bool GetInt(IntSetting key, int* result) override {
    // We don't ever have any integers. (See AddProxy() below about ports.)
    return false;
  }
  bool GetStringList(StringListSetting key,
                     std::vector<std::string>* result) override {
    auto it = strings_table_.find(key);
    if (it == strings_table_.end())
      return false;
    *result = it->second;
    return true;
  }

  bool BypassListIsReversed() override { return reversed_bypass_list_; }

  ProxyBypassRules::ParseFormat GetBypassListFormat() override {
    return ProxyBypassRules::ParseFormat::kHostnameSuffixMatching;
  }

 private:
  void ResetCachedSettings() {
    string_table_.clear();
    strings_table_.clear();
    indirect_manual_ = false;
    auto_no_pac_ = false;
    reversed_bypass_list_ = false;
  }

  base::FilePath KDEHomeToConfigPath(const base::FilePath& kde_home) {
    return kde_home.Append("share").Append("config");
  }

  void AddProxy(StringSetting host_key, const std::string& value) {
    if (value.empty() || value.substr(0, 3) == "//:")
      // No proxy.
      return;
    size_t space = value.find(' ');
    if (space != std::string::npos) {
      // Newer versions of KDE use a space rather than a colon to separate the
      // port number from the hostname. If we find this, we need to convert it.
      std::string fixed = value;
      fixed[space] = ':';
      string_table_[host_key] = fixed;
    } else {
      // We don't need to parse the port number out; GetProxyFromSettings()
      // would only append it right back again. So we just leave the port
      // number right in the host string.
      string_table_[host_key] = value;
    }
  }

  void AddHostList(StringListSetting key, const std::string& value) {
    std::vector<std::string> tokens;
    base::StringTokenizer tk(value, ", ");
    while (tk.GetNext()) {
      std::string token = tk.token();
      if (!token.empty())
        tokens.push_back(token);
    }
    strings_table_[key] = tokens;
  }

  void AddKDESetting(const std::string& key, const std::string& value) {
    if (key == "ProxyType") {
      const char* mode = "none";
      indirect_manual_ = false;
      auto_no_pac_ = false;
      int int_value = StringToIntOrDefault(value, 0);
      switch (int_value) {
        case 1:  // Manual configuration.
          mode = "manual";
          break;
        case 2:  // PAC URL.
          mode = "auto";
          break;
        case 3:  // WPAD.
          mode = "auto";
          auto_no_pac_ = true;
          break;
        case 4:  // Indirect manual via environment variables.
          mode = "manual";
          indirect_manual_ = true;
          break;
        default:  // No proxy, or maybe kioslaverc syntax error.
          break;
      }
      string_table_[PROXY_MODE] = mode;
    } else if (key == "Proxy Config Script") {
      string_table_[PROXY_AUTOCONF_URL] = value;
    } else if (key == "httpProxy") {
      AddProxy(PROXY_HTTP_HOST, value);
    } else if (key == "httpsProxy") {
      AddProxy(PROXY_HTTPS_HOST, value);
    } else if (key == "ftpProxy") {
      AddProxy(PROXY_FTP_HOST, value);
    } else if (key == "socksProxy") {
      // Older versions of KDE configure SOCKS in a weird way involving
      // LD_PRELOAD and a library that intercepts network calls to SOCKSify
      // them. We don't support it. KDE 4.8 added a proper SOCKS setting.
      AddProxy(PROXY_SOCKS_HOST, value);
    } else if (key == "ReversedException") {
      // We count "true" or any nonzero number as true, otherwise false.
      // A failure parsing the integer will also mean false.
      reversed_bypass_list_ =
          (value == "true" || StringToIntOrDefault(value, 0) != 0);
    } else if (key == "NoProxyFor") {
      AddHostList(PROXY_IGNORE_HOSTS, value);
    } else if (key == "AuthMode") {
      // Check for authentication, just so we can warn.
      int mode = StringToIntOrDefault(value, 0);
      if (mode) {
        // ProxyConfig does not support authentication parameters, but
        // Chrome will prompt for the password later. So we ignore this.
        LOG(WARNING) <<
            "Proxy authentication parameters ignored, see bug 16709";
      }
    }
  }

  void ResolveIndirect(StringSetting key) {
    auto it = string_table_.find(key);
    if (it != string_table_.end()) {
      std::string value;
      if (env_var_getter_->GetVar(it->second.c_str(), &value))
        it->second = value;
      else
        string_table_.erase(it);
    }
  }

  void ResolveIndirectList(StringListSetting key) {
    auto it = strings_table_.find(key);
    if (it != strings_table_.end()) {
      std::string value;
      if (!it->second.empty() &&
          env_var_getter_->GetVar(it->second[0].c_str(), &value))
        AddHostList(key, value);
      else
        strings_table_.erase(it);
    }
  }

  // The settings in kioslaverc could occur in any order, but some affect
  // others. Rather than read the whole file in and then query them in an
  // order that allows us to handle that, we read the settings in whatever
  // order they occur and do any necessary tweaking after we finish.
  void ResolveModeEffects() {
    if (indirect_manual_) {
      ResolveIndirect(PROXY_HTTP_HOST);
      ResolveIndirect(PROXY_HTTPS_HOST);
      ResolveIndirect(PROXY_FTP_HOST);
      ResolveIndirectList(PROXY_IGNORE_HOSTS);
    }
    if (auto_no_pac_) {
      // Remove the PAC URL; we're not supposed to use it.
      string_table_.erase(PROXY_AUTOCONF_URL);
    }
  }

  // Reads kioslaverc one line at a time and calls AddKDESetting() to add
  // each relevant name-value pair to the appropriate value table.
  void UpdateCachedSettings() {
    base::FilePath kioslaverc = kde_config_dir_.Append("kioslaverc");
    base::ScopedFILE input(base::OpenFile(kioslaverc, "r"));
    if (!input.get())
      return;
    ResetCachedSettings();
    bool in_proxy_settings = false;
    bool line_too_long = false;
    char line[BUFFER_SIZE];
    // fgets() will return NULL on EOF or error.
    while (fgets(line, sizeof(line), input.get())) {
      // fgets() guarantees the line will be properly terminated.
      size_t length = strlen(line);
      if (!length)
        continue;
      // This should be true even with CRLF endings.
      if (line[length - 1] != '\n') {
        line_too_long = true;
        continue;
      }
      if (line_too_long) {
        // The previous line had no line ending, but this done does. This is
        // the end of the line that was too long, so warn here and skip it.
        LOG(WARNING) << "skipped very long line in " << kioslaverc.value();
        line_too_long = false;
        continue;
      }
      // Remove the LF at the end, and the CR if there is one.
      line[--length] = '\0';
      if (length && line[length - 1] == '\r')
        line[--length] = '\0';
      // Now parse the line.
      if (line[0] == '[') {
        // Switching sections. All we care about is whether this is
        // the (a?) proxy settings section, for both KDE3 and KDE4.
        in_proxy_settings = !strncmp(line, "[Proxy Settings]", 16);
      } else if (in_proxy_settings) {
        // A regular line, in the (a?) proxy settings section.
        char* split = strchr(line, '=');
        // Skip this line if it does not contain an = sign.
        if (!split)
          continue;
        // Split the line on the = and advance |split|.
        *(split++) = 0;
        std::string key = line;
        std::string value = split;
        base::TrimWhitespaceASCII(key, base::TRIM_ALL, &key);
        base::TrimWhitespaceASCII(value, base::TRIM_ALL, &value);
        // Skip this line if the key name is empty.
        if (key.empty())
          continue;
        // Is the value name localized?
        if (key[key.length() - 1] == ']') {
          // Find the matching bracket.
          length = key.rfind('[');
          // Skip this line if the localization indicator is malformed.
          if (length == std::string::npos)
            continue;
          // Trim the localization indicator off.
          key.resize(length);
          // Remove any resulting trailing whitespace.
          base::TrimWhitespaceASCII(key, base::TRIM_TRAILING, &key);
          // Skip this line if the key name is now empty.
          if (key.empty())
            continue;
        }
        // Now fill in the tables.
        AddKDESetting(key, value);
      }
    }
    if (ferror(input.get()))
      LOG(ERROR) << "error reading " << kioslaverc.value();
    ResolveModeEffects();
  }

  // This is the callback from the debounce timer.
  void OnDebouncedNotification() {
    DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
    VLOG(1) << "inotify change notification for kioslaverc";
    UpdateCachedSettings();
    CHECK(notify_delegate_);
    // Forward to a method on the proxy config service delegate object.
    notify_delegate_->OnCheckProxyConfigSettings();
  }

  // Called by OnFileCanReadWithoutBlocking() on the file thread. Reads
  // from the inotify file descriptor and starts up a debounce timer if
  // an event for kioslaverc is seen.
  void OnChangeNotification() {
    DCHECK_GE(inotify_fd_,  0);
    DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
    char event_buf[(sizeof(inotify_event) + NAME_MAX + 1) * 4];
    bool kioslaverc_touched = false;
    ssize_t r;
    while ((r = read(inotify_fd_, event_buf, sizeof(event_buf))) > 0) {
      // inotify returns variable-length structures, which is why we have
      // this strange-looking loop instead of iterating through an array.
      char* event_ptr = event_buf;
      while (event_ptr < event_buf + r) {
        inotify_event* event = reinterpret_cast<inotify_event*>(event_ptr);
        // The kernel always feeds us whole events.
        CHECK_LE(event_ptr + sizeof(inotify_event), event_buf + r);
        CHECK_LE(event->name + event->len, event_buf + r);
        if (!strcmp(event->name, "kioslaverc"))
          kioslaverc_touched = true;
        // Advance the pointer just past the end of the filename.
        event_ptr = event->name + event->len;
      }
      // We keep reading even if |kioslaverc_touched| is true to drain the
      // inotify event queue.
    }
    if (!r)
      // Instead of returning -1 and setting errno to EINVAL if there is not
      // enough buffer space, older kernels (< 2.6.21) return 0. Simulate the
      // new behavior (EINVAL) so we can reuse the code below.
      errno = EINVAL;
    if (errno != EAGAIN) {
      PLOG(WARNING) << "error reading inotify file descriptor";
      if (errno == EINVAL) {
        // Our buffer is not large enough to read the next event. This should
        // not happen (because its size is calculated to always be sufficiently
        // large), but if it does we'd warn continuously since |inotify_fd_|
        // would be forever ready to read. Close it and stop watching instead.
        LOG(ERROR) << "inotify failure; no longer watching kioslaverc!";
        inotify_watcher_.reset();
        close(inotify_fd_);
        inotify_fd_ = -1;
      }
    }
    if (kioslaverc_touched) {
      LOG(ERROR) << "kioslaverc_touched";
      // We don't use Reset() because the timer may not yet be running.
      // (In that case Stop() is a no-op.)
      debounce_timer_->Stop();
      debounce_timer_->Start(FROM_HERE, base::TimeDelta::FromMilliseconds(
          kDebounceTimeoutMilliseconds), this,
          &SettingGetterImplKDE::OnDebouncedNotification);
    }
  }

  typedef std::map<StringSetting, std::string> string_map_type;
  typedef std::map<StringListSetting,
                   std::vector<std::string> > strings_map_type;

  int inotify_fd_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> inotify_watcher_;
  ProxyConfigServiceLinux::Delegate* notify_delegate_;
  std::unique_ptr<base::OneShotTimer> debounce_timer_;
  base::FilePath kde_config_dir_;
  bool indirect_manual_;
  bool auto_no_pac_;
  bool reversed_bypass_list_;
  // We don't own |env_var_getter_|.  It's safe to hold a pointer to it, since
  // both it and us are owned by ProxyConfigServiceLinux::Delegate, and have the
  // same lifetime.
  base::Environment* env_var_getter_;

  // We cache these settings whenever we re-read the kioslaverc file.
  string_map_type string_table_;
  strings_map_type strings_table_;

  // Task runner for doing blocking file IO on, as well as handling inotify
  // events on.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SettingGetterImplKDE);
};

}  // namespace

bool ProxyConfigServiceLinux::Delegate::GetProxyFromSettings(
    SettingGetter::StringSetting host_key,
    ProxyServer* result_server) {
  std::string host;
  if (!setting_getter_->GetString(host_key, &host) || host.empty()) {
    // Unset or empty.
    return false;
  }
  // Check for an optional port.
  int port = 0;
  SettingGetter::IntSetting port_key =
      SettingGetter::HostSettingToPortSetting(host_key);
  setting_getter_->GetInt(port_key, &port);
  if (port != 0) {
    // If a port is set and non-zero:
    host += ":" + base::NumberToString(port);
  }

  // gsettings settings do not appear to distinguish between SOCKS version. We
  // default to version 5. For more information on this policy decision, see:
  // http://code.google.com/p/chromium/issues/detail?id=55912#c2
  ProxyServer::Scheme scheme = (host_key == SettingGetter::PROXY_SOCKS_HOST) ?
      ProxyServer::SCHEME_SOCKS5 : ProxyServer::SCHEME_HTTP;
  host = FixupProxyHostScheme(scheme, host);
  ProxyServer proxy_server = ProxyServer::FromURI(host,
                                                  ProxyServer::SCHEME_HTTP);
  if (proxy_server.is_valid()) {
    *result_server = proxy_server;
    return true;
  }
  return false;
}

base::Optional<ProxyConfigWithAnnotation>
ProxyConfigServiceLinux::Delegate::GetConfigFromSettings() {
  ProxyConfig config;

  std::string mode;
  if (!setting_getter_->GetString(SettingGetter::PROXY_MODE, &mode)) {
    // We expect this to always be set, so if we don't see it then we probably
    // have a gsettings problem, and so we don't have a valid proxy config.
    return base::nullopt;
  }
  if (mode == "none") {
    // Specifically specifies no proxy.
    return ProxyConfigWithAnnotation(
        config, NetworkTrafficAnnotationTag(traffic_annotation_));
  }

  if (mode == "auto") {
    // Automatic proxy config.
    std::string pac_url_str;
    if (setting_getter_->GetString(SettingGetter::PROXY_AUTOCONF_URL,
                                   &pac_url_str)) {
      if (!pac_url_str.empty()) {
        // If the PAC URL is actually a file path, then put file:// in front.
        if (pac_url_str[0] == '/')
          pac_url_str = "file://" + pac_url_str;
        GURL pac_url(pac_url_str);
        if (!pac_url.is_valid())
          return base::nullopt;
        config.set_pac_url(pac_url);
        return ProxyConfigWithAnnotation(
            config, NetworkTrafficAnnotationTag(traffic_annotation_));
      }
    }
    config.set_auto_detect(true);
    return ProxyConfigWithAnnotation(
        config, NetworkTrafficAnnotationTag(traffic_annotation_));
  }

  if (mode != "manual") {
    // Mode is unrecognized.
    return base::nullopt;
  }
  bool use_http_proxy;
  if (setting_getter_->GetBool(SettingGetter::PROXY_USE_HTTP_PROXY,
                               &use_http_proxy)
      && !use_http_proxy) {
    // Another master switch for some reason. If set to false, then no
    // proxy. But we don't panic if the key doesn't exist.
    return ProxyConfigWithAnnotation(
        config, NetworkTrafficAnnotationTag(traffic_annotation_));
  }

  bool same_proxy = false;
  // Indicates to use the http proxy for all protocols. This one may
  // not exist (presumably on older versions); we assume false in that
  // case.
  setting_getter_->GetBool(SettingGetter::PROXY_USE_SAME_PROXY,
                           &same_proxy);

  ProxyServer proxy_for_http;
  ProxyServer proxy_for_https;
  ProxyServer proxy_for_ftp;
  ProxyServer socks_proxy;  // (socks)

  // This counts how many of the above ProxyServers were defined and valid.
  size_t num_proxies_specified = 0;

  // Extract the per-scheme proxies. If we failed to parse it, or no proxy was
  // specified for the scheme, then the resulting ProxyServer will be invalid.
  if (GetProxyFromSettings(SettingGetter::PROXY_HTTP_HOST, &proxy_for_http))
    num_proxies_specified++;
  if (GetProxyFromSettings(SettingGetter::PROXY_HTTPS_HOST, &proxy_for_https))
    num_proxies_specified++;
  if (GetProxyFromSettings(SettingGetter::PROXY_FTP_HOST, &proxy_for_ftp))
    num_proxies_specified++;
  if (GetProxyFromSettings(SettingGetter::PROXY_SOCKS_HOST, &socks_proxy))
    num_proxies_specified++;

  if (same_proxy) {
    if (proxy_for_http.is_valid()) {
      // Use the http proxy for all schemes.
      config.proxy_rules().type = ProxyConfig::ProxyRules::Type::PROXY_LIST;
      config.proxy_rules().single_proxies.SetSingleProxyServer(proxy_for_http);
    }
  } else if (num_proxies_specified > 0) {
    if (socks_proxy.is_valid() && num_proxies_specified == 1) {
      // If the only proxy specified was for SOCKS, use it for all schemes.
      config.proxy_rules().type = ProxyConfig::ProxyRules::Type::PROXY_LIST;
      config.proxy_rules().single_proxies.SetSingleProxyServer(socks_proxy);
    } else {
      // Otherwise use the indicated proxies per-scheme.
      config.proxy_rules().type =
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
      config.proxy_rules().proxies_for_http.SetSingleProxyServer(
          proxy_for_http);
      config.proxy_rules().proxies_for_https.SetSingleProxyServer(
          proxy_for_https);
      config.proxy_rules().proxies_for_ftp.SetSingleProxyServer(proxy_for_ftp);
      config.proxy_rules().fallback_proxies.SetSingleProxyServer(socks_proxy);
    }
  }

  if (config.proxy_rules().empty()) {
    // Manual mode but we couldn't parse any rules.
    return base::nullopt;
  }

  // Check for authentication, just so we can warn.
  bool use_auth = false;
  setting_getter_->GetBool(SettingGetter::PROXY_USE_AUTHENTICATION,
                           &use_auth);
  if (use_auth) {
    // ProxyConfig does not support authentication parameters, but
    // Chrome will prompt for the password later. So we ignore
    // /system/http_proxy/*auth* settings.
    LOG(WARNING) << "Proxy authentication parameters ignored, see bug 16709";
  }

  // Now the bypass list.
  auto format = setting_getter_->GetBypassListFormat();

  std::vector<std::string> ignore_hosts_list;
  config.proxy_rules().bypass_rules.Clear();
  if (setting_getter_->GetStringList(SettingGetter::PROXY_IGNORE_HOSTS,
                                     &ignore_hosts_list)) {
    for (const auto& rule : ignore_hosts_list) {
      config.proxy_rules().bypass_rules.AddRuleFromString(rule, format);
    }
  }
  // Note that there are no settings with semantics corresponding to
  // bypass of local names in GNOME. In KDE, "<local>" is supported
  // as a hostname rule.

  // KDE allows one to reverse the bypass rules.
  config.proxy_rules().reverse_bypass = setting_getter_->BypassListIsReversed();

  return ProxyConfigWithAnnotation(
      config, NetworkTrafficAnnotationTag(traffic_annotation_));
}

ProxyConfigServiceLinux::Delegate::Delegate(
    std::unique_ptr<base::Environment> env_var_getter,
    base::Optional<std::unique_ptr<SettingGetter>> setting_getter,
    base::Optional<NetworkTrafficAnnotationTag> traffic_annotation)
    : env_var_getter_(std::move(env_var_getter)) {
  if (traffic_annotation) {
    traffic_annotation_ =
        MutableNetworkTrafficAnnotationTag(traffic_annotation.value());
  }

  if (setting_getter) {
    setting_getter_ = std::move(setting_getter.value());
    return;
  }

  // Figure out which SettingGetterImpl to use, if any.
  switch (base::nix::GetDesktopEnvironment(env_var_getter_.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_CINNAMON:
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_PANTHEON:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
#if defined(USE_GIO)
      {
      std::unique_ptr<SettingGetterImplGSettings> gs_getter(
          new SettingGetterImplGSettings());
      // We have to load symbols and check the GNOME version in use to decide
      // if we should use the gsettings getter. See CheckVersion().
      if (gs_getter->CheckVersion(env_var_getter_.get()))
        setting_getter_ = std::move(gs_getter);
      }
#endif
      break;
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
      setting_getter_.reset(new SettingGetterImplKDE(env_var_getter_.get()));
      break;
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      break;
  }
}

void ProxyConfigServiceLinux::Delegate::SetUpAndFetchInitialConfig(
    const scoped_refptr<base::SingleThreadTaskRunner>& glib_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  traffic_annotation_ = MutableNetworkTrafficAnnotationTag(traffic_annotation);

  // We should be running on the default glib main loop thread right
  // now. gsettings can only be accessed from this thread.
  DCHECK(glib_task_runner->RunsTasksInCurrentSequence());
  glib_task_runner_ = glib_task_runner;
  main_task_runner_ = main_task_runner;

  // If we are passed a NULL |main_task_runner|, then don't set up proxy
  // setting change notifications. This should not be the usual case but is
  // intended to/ simplify test setups.
  if (!main_task_runner_.get())
    VLOG(1) << "Monitoring of proxy setting changes is disabled";

  // Fetch and cache the current proxy config. The config is left in
  // cached_config_, where GetLatestProxyConfig() running on the main TaskRunner
  // will expect to find it. This is safe to do because we return
  // before this ProxyConfigServiceLinux is passed on to
  // the ProxyResolutionService.

  // Note: It would be nice to prioritize environment variables
  // and only fall back to gsettings if env vars were unset. But
  // gnome-terminal "helpfully" sets http_proxy and no_proxy, and it
  // does so even if the proxy mode is set to auto, which would
  // mislead us.

  cached_config_ = base::nullopt;
  if (setting_getter_ && setting_getter_->Init(glib_task_runner)) {
    cached_config_ = GetConfigFromSettings();
  }
  if (cached_config_) {
    VLOG(1) << "Obtained proxy settings from annotation hash code "
            << cached_config_->traffic_annotation().unique_id_hash_code;

    // If gsettings proxy mode is "none", meaning direct, then we take
    // that to be a valid config and will not check environment
    // variables. The alternative would have been to look for a proxy
    // wherever we can find one.

    // Keep a copy of the config for use from this thread for
    // comparison with updated settings when we get notifications.
    reference_config_ = cached_config_;

    // We only set up notifications if we have the main and file loops
    // available. We do this after getting the initial configuration so that we
    // don't have to worry about cancelling it if the initial fetch above fails.
    // Note that setting up notifications has the side effect of simulating a
    // change, so that we won't lose any updates that may have happened after
    // the initial fetch and before setting up notifications. We'll detect the
    // common case of no changes in OnCheckProxyConfigSettings() (or sooner) and
    // ignore it.
    if (main_task_runner.get()) {
      scoped_refptr<base::SequencedTaskRunner> required_loop =
          setting_getter_->GetNotificationTaskRunner();
      if (!required_loop.get() || required_loop->RunsTasksInCurrentSequence()) {
        // In this case we are already on an acceptable thread.
        SetUpNotifications();
      } else {
        // Post a task to set up notifications. We don't wait for success.
        required_loop->PostTask(
            FROM_HERE,
            base::BindOnce(
                &ProxyConfigServiceLinux::Delegate::SetUpNotifications, this));
      }
    }
  }

  if (!cached_config_) {
    // We fall back on environment variables.
    //
    // Consulting environment variables doesn't need to be done from the
    // default glib main loop, but it's a tiny enough amount of work.
    cached_config_ = GetConfigFromEnv();
    if (cached_config_) {
      VLOG(1) << "Obtained proxy settings from environment variables";
    }
  }
}

// Depending on the SettingGetter in use, this method will be called
// on either the UI thread (GSettings) or the file thread (KDE).
void ProxyConfigServiceLinux::Delegate::SetUpNotifications() {
  scoped_refptr<base::SequencedTaskRunner> required_loop =
      setting_getter_->GetNotificationTaskRunner();
  DCHECK(!required_loop.get() || required_loop->RunsTasksInCurrentSequence());
  if (!setting_getter_->SetUpNotifications(this))
    LOG(ERROR) << "Unable to set up proxy configuration change notifications";
}

void ProxyConfigServiceLinux::Delegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProxyConfigServiceLinux::Delegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ProxyConfigService::ConfigAvailability
ProxyConfigServiceLinux::Delegate::GetLatestProxyConfig(
    ProxyConfigWithAnnotation* config) {
  // This is called from the main TaskRunner.
  DCHECK(!main_task_runner_.get() ||
         main_task_runner_->RunsTasksInCurrentSequence());

  // Simply return the last proxy configuration that glib_default_loop
  // notified us of.
  *config = GetConfigOrDirect(cached_config_);

  // We return CONFIG_VALID to indicate that *config was filled in. It is always
  // going to be available since we initialized eagerly on the UI thread.
  // TODO(eroman): do lazy initialization instead, so we no longer need
  //               to construct ProxyConfigServiceLinux on the UI thread.
  //               In which case, we may return false here.
  return CONFIG_VALID;
}

// Depending on the SettingGetter in use, this method will be called
// on either the UI thread (GSettings) or the file thread (KDE).
void ProxyConfigServiceLinux::Delegate::OnCheckProxyConfigSettings() {
  scoped_refptr<base::SequencedTaskRunner> required_loop =
      setting_getter_->GetNotificationTaskRunner();
  DCHECK(!required_loop.get() || required_loop->RunsTasksInCurrentSequence());
  base::Optional<ProxyConfigWithAnnotation> new_config =
      GetConfigFromSettings();

  // See if it is different from what we had before.
  if (new_config.has_value() != reference_config_.has_value() ||
      (new_config.has_value() &&
       !new_config->value().Equals(reference_config_->value()))) {
    // Post a task to the main TaskRunner with the new configuration, so it can
    // update |cached_config_|.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ProxyConfigServiceLinux::Delegate::SetNewProxyConfig,
                       this, new_config));
    // Update the thread-private copy in |reference_config_| as well.
    reference_config_ = new_config;
  } else {
    VLOG(1) << "Detected no-op change to proxy settings. Doing nothing.";
  }
}

void ProxyConfigServiceLinux::Delegate::SetNewProxyConfig(
    const base::Optional<ProxyConfigWithAnnotation>& new_config) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  VLOG(1) << "Proxy configuration changed";
  cached_config_ = new_config;
  for (auto& observer : observers_) {
    observer.OnProxyConfigChanged(GetConfigOrDirect(new_config),
                                  ProxyConfigService::CONFIG_VALID);
  }
}

void ProxyConfigServiceLinux::Delegate::PostDestroyTask() {
  if (!setting_getter_)
    return;

  scoped_refptr<base::SequencedTaskRunner> shutdown_loop =
      setting_getter_->GetNotificationTaskRunner();
  if (!shutdown_loop.get() || shutdown_loop->RunsTasksInCurrentSequence()) {
    // Already on the right thread, call directly.
    // This is the case for the unittests.
    OnDestroy();
  } else {
    // Post to shutdown thread. Note that on browser shutdown, we may quit
    // this MessageLoop and exit the program before ever running this.
    shutdown_loop->PostTask(
        FROM_HERE,
        base::BindOnce(&ProxyConfigServiceLinux::Delegate::OnDestroy, this));
  }
}
void ProxyConfigServiceLinux::Delegate::OnDestroy() {
  scoped_refptr<base::SequencedTaskRunner> shutdown_loop =
      setting_getter_->GetNotificationTaskRunner();
  DCHECK(!shutdown_loop.get() || shutdown_loop->RunsTasksInCurrentSequence());
  setting_getter_->ShutDown();
}

ProxyConfigServiceLinux::ProxyConfigServiceLinux()
    : delegate_(new Delegate(base::Environment::Create(),
                             base::nullopt,
                             base::nullopt)) {}

ProxyConfigServiceLinux::~ProxyConfigServiceLinux() {
  delegate_->PostDestroyTask();
}

ProxyConfigServiceLinux::ProxyConfigServiceLinux(
    std::unique_ptr<base::Environment> env_var_getter,
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : delegate_(new Delegate(std::move(env_var_getter),
                             base::nullopt,
                             traffic_annotation)) {}

ProxyConfigServiceLinux::ProxyConfigServiceLinux(
    std::unique_ptr<base::Environment> env_var_getter,
    SettingGetter* setting_getter,
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : delegate_(new Delegate(std::move(env_var_getter),
                             base::WrapUnique(setting_getter),
                             traffic_annotation)) {}

void ProxyConfigServiceLinux::AddObserver(Observer* observer) {
  delegate_->AddObserver(observer);
}

void ProxyConfigServiceLinux::RemoveObserver(Observer* observer) {
  delegate_->RemoveObserver(observer);
}

ProxyConfigService::ConfigAvailability
ProxyConfigServiceLinux::GetLatestProxyConfig(
    ProxyConfigWithAnnotation* config) {
  return delegate_->GetLatestProxyConfig(config);
}

}  // namespace net
