// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PAC_FILE_DECIDER_H_
#define NET_PROXY_RESOLUTION_PAC_FILE_DECIDER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace net {

class DhcpPacFileFetcher;
class NetLog;
class ProxyResolver;
class PacFileFetcher;

// Structure that encapsulates the result a PacFileData along with an
// indication of its origin: was it obtained implicitly from auto-detect,
// or was it read from a more explicitly configured URL.
//
// Note that |!from_auto_detect| does NOT imply the script was securely
// delivered. Most commonly PAC scripts are configured from http:// URLs,
// both for auto-detect and not.
struct NET_EXPORT_PRIVATE PacFileDataWithSource {
  PacFileDataWithSource();
  explicit PacFileDataWithSource(const PacFileDataWithSource&);
  ~PacFileDataWithSource();

  PacFileDataWithSource& operator=(const PacFileDataWithSource&);

  scoped_refptr<PacFileData> data;
  bool from_auto_detect = false;
};

// PacFileDecider is a helper class used by ProxyResolutionService to
// determine which PAC script to use given our proxy configuration.
//
// This involves trying to use PAC scripts in this order:
//
//   (1) WPAD (DHCP) if auto-detect is on.
//   (2) WPAD (DNS) if auto-detect is on.
//   (3) Custom PAC script if a URL was given.
//
// If no PAC script was successfully selected, then it fails with either a
// network error, or PAC_SCRIPT_FAILED (indicating it did not pass our
// validation).
//
// On successful completion, the fetched PAC script data can be accessed using
// script_data().
//
// Deleting PacFileDecider while Init() is in progress, will
// cancel the request.
//
class NET_EXPORT_PRIVATE PacFileDecider {
 public:
  // |pac_file_fetcher|, |dhcp_pac_file_fetcher| and
  // |net_log| must remain valid for the lifespan of PacFileDecider.
  PacFileDecider(PacFileFetcher* pac_file_fetcher,
                 DhcpPacFileFetcher* dhcp_pac_file_fetcher,
                 NetLog* net_log);

  // Aborts any in-progress request.
  ~PacFileDecider();

  // Evaluates the effective proxy settings for |config|, and downloads the
  // associated PAC script.
  // If |wait_delay| is positive, the initialization will pause for this
  // amount of time before getting started.
  // On successful completion, the "effective" proxy settings we ended up
  // deciding on will be available vial the effective_settings() accessor.
  // Note that this may differ from |config| since we will have stripped any
  // manual settings, and decided whether to use auto-detect or the custom PAC
  // URL. Finally, if auto-detect was used we may now have resolved that to a
  // specific script URL.
  int Start(const ProxyConfigWithAnnotation& config,
            const base::TimeDelta wait_delay,
            bool fetch_pac_bytes,
            CompletionOnceCallback callback);

  // Shuts down any in-progress DNS requests, and cancels any ScriptFetcher
  // requests. Does not call OnShutdown() on the [Dhcp]PacFileFetcher. Any
  // pending callback will not be invoked.
  void OnShutdown();

  const ProxyConfigWithAnnotation& effective_config() const;

  const PacFileDataWithSource& script_data() const;

  void set_quick_check_enabled(bool enabled) { quick_check_enabled_ = enabled; }

  bool quick_check_enabled() const { return quick_check_enabled_; }

 private:
  // Represents the sources from which we can get PAC files; two types of
  // auto-detect or a custom URL.
  struct PacSource {
    enum Type { WPAD_DHCP, WPAD_DNS, CUSTOM };

    PacSource(Type type, const GURL& url) : type(type), url(url) {}

    // Returns a Value representing the PacSource.  |effective_pac_url| is the
    // URL derived from information contained in
    // |this|, if Type is not WPAD_DHCP.
    base::Value NetLogParams(const GURL& effective_pac_url) const;

    Type type;
    GURL url;  // Empty unless |type == PAC_SOURCE_CUSTOM|.
  };

  typedef std::vector<PacSource> PacSourceList;

  enum State {
    STATE_NONE,
    STATE_WAIT,
    STATE_WAIT_COMPLETE,
    STATE_QUICK_CHECK,
    STATE_QUICK_CHECK_COMPLETE,
    STATE_FETCH_PAC_SCRIPT,
    STATE_FETCH_PAC_SCRIPT_COMPLETE,
    STATE_VERIFY_PAC_SCRIPT,
    STATE_VERIFY_PAC_SCRIPT_COMPLETE,
  };

  // Returns ordered list of PAC urls to try for |config|.
  PacSourceList BuildPacSourcesFallbackList(const ProxyConfig& config) const;

  void OnIOCompletion(int result);
  int DoLoop(int result);

  int DoWait();
  int DoWaitComplete(int result);

  int DoQuickCheck();
  int DoQuickCheckComplete(int result);

  int DoFetchPacScript();
  int DoFetchPacScriptComplete(int result);

  int DoVerifyPacScript();
  int DoVerifyPacScriptComplete(int result);

  // Tries restarting using the next fallback PAC URL:
  // |pac_sources_[++current_pac_source_index]|.
  // Returns OK and rewinds the state machine when there
  // is something to try, otherwise returns |error|.
  int TryToFallbackPacSource(int error);

  // Gets the initial state (we skip fetching when the
  // ProxyResolver doesn't |expect_pac_bytes()|.
  State GetStartState() const;

  void DetermineURL(const PacSource& pac_source, GURL* effective_pac_url);

  // Returns the current PAC URL we are fetching/testing.
  const PacSource& current_pac_source() const;

  void OnWaitTimerFired();
  void DidComplete();
  void Cancel();

  PacFileFetcher* pac_file_fetcher_;
  DhcpPacFileFetcher* dhcp_pac_file_fetcher_;

  CompletionOnceCallback callback_;

  size_t current_pac_source_index_;

  // Filled when the PAC script fetch completes.
  base::string16 pac_script_;

  // Flag indicating whether the caller requested a mandatory PAC script
  // (i.e. fallback to direct connections are prohibited).
  bool pac_mandatory_;

  // Whether we have an existing custom PAC URL.
  bool have_custom_pac_url_;

  PacSourceList pac_sources_;
  State next_state_;

  NetLogWithSource net_log_;

  bool fetch_pac_bytes_;

  base::TimeDelta wait_delay_;
  base::OneShotTimer wait_timer_;

  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  // Whether to do DNS quick check
  bool quick_check_enabled_;

  // Results.
  ProxyConfigWithAnnotation effective_config_;
  PacFileDataWithSource script_data_;

  std::unique_ptr<HostResolver::ResolveHostRequest> resolve_request_;

  base::OneShotTimer quick_check_timer_;

  DISALLOW_COPY_AND_ASSIGN(PacFileDecider);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PAC_FILE_DECIDER_H_
