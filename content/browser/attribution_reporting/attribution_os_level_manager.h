// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/content_browser_client.h"

namespace base {
class Time;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

struct OsRegistration;
class WebContents;

// Interface between the browser's Attribution Reporting implementation and the
// operating system's.
class CONTENT_EXPORT AttributionOsLevelManager {
 public:
  using ApiState = ContentBrowserClient::AttributionReportingOsApiState;

  class CONTENT_EXPORT ScopedApiStateForTesting {
   public:
    explicit ScopedApiStateForTesting(std::optional<ApiState>);
    ~ScopedApiStateForTesting();

    ScopedApiStateForTesting(const ScopedApiStateForTesting&) = delete;
    ScopedApiStateForTesting& operator=(const ScopedApiStateForTesting&) =
        delete;

    ScopedApiStateForTesting(ScopedApiStateForTesting&&) = delete;
    ScopedApiStateForTesting& operator=(ScopedApiStateForTesting&&) = delete;

   private:
    const std::optional<ApiState> previous_;
  };

  static ApiState GetApiState();
  static void SetApiState(std::optional<ApiState>);

  [[nodiscard]] static ContentBrowserClient::AttributionReportingOsRegistrars
  GetAttributionReportingOsRegistrars(WebContents*);

  virtual ~AttributionOsLevelManager() = default;

  using RegisterCallback =
      base::OnceCallback<void(const OsRegistration&,
                              const std::vector<bool>& success)>;

  virtual void Register(OsRegistration,
                        const std::vector<bool>& is_debug_key_allowed,
                        RegisterCallback) = 0;

  // Clears storage data with the OS.
  // Note that `done` is not run if `AttributionOsLevelManager` is destroyed
  // first.
  virtual void ClearData(base::Time delete_begin,
                         base::Time delete_end,
                         const std::set<url::Origin>& origins,
                         const std::set<std::string>& domains,
                         BrowsingDataFilterBuilder::Mode mode,
                         bool delete_rate_limit_data,
                         base::OnceClosure done) = 0;

 protected:
  [[nodiscard]] static bool ShouldInitializeApiState();
};

class CONTENT_EXPORT NoOpAttributionOsLevelManager
    : public AttributionOsLevelManager {
 public:
  ~NoOpAttributionOsLevelManager() override;

  void Register(OsRegistration,
                const std::vector<bool>& is_debug_key_allowed,
                RegisterCallback) override;

  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 const std::set<url::Origin>& origins,
                 const std::set<std::string>& domains,
                 BrowsingDataFilterBuilder::Mode mode,
                 bool delete_rate_limit_data,
                 base::OnceClosure done) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_H_
