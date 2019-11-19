// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_MESSAGE_FILTER_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

class GURL;

namespace base {
class DictionaryValue;
}  // namespace base

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace storage {
class DatabaseTracker;
class QuotaManager;
}  // namespace storage

namespace content {

class WebTestMessageFilter : public BrowserMessageFilter {
 public:
  WebTestMessageFilter(int render_process_id,
                       storage::DatabaseTracker* database_tracker,
                       storage::QuotaManager* quota_manager,
                       network::mojom::NetworkContext* network_context);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<WebTestMessageFilter>;

  ~WebTestMessageFilter() override;

  // BrowserMessageFilter implementation.
  void OnDestruct() const override;
  scoped_refptr<base::SequencedTaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnReadFileToString(const base::FilePath& local_file,
                          std::string* contents);
  void OnRegisterIsolatedFileSystem(
      const std::vector<base::FilePath>& absolute_filenames,
      std::string* filesystem_id);
  void OnClearAllDatabases();
  void OnSetDatabaseQuota(int quota);
  void OnSimulateWebNotificationClick(
      const std::string& title,
      const base::Optional<int>& action_index,
      const base::Optional<base::string16>& reply);
  void OnSimulateWebNotificationClose(const std::string& title, bool by_user);
  void OnSimulateWebContentIndexDelete(const std::string& id);
  void OnDeleteAllCookies();
  void OnSetPermission(const std::string& name,
                       blink::mojom::PermissionStatus status,
                       const GURL& origin,
                       const GURL& embedding_origin);
  void OnResetPermissions();
  void OnWebTestRuntimeFlagsChanged(
      const base::DictionaryValue& changed_web_test_runtime_flags);
  void OnTestFinishedInSecondaryRenderer();
  void OnInitiateCaptureDump(bool capture_navigation_history,
                             bool capture_pixels);
  void OnInspectSecondaryWindow();

  int render_process_id_;

  scoped_refptr<storage::DatabaseTracker> database_tracker_;
  scoped_refptr<storage::QuotaManager> quota_manager_;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;

  DISALLOW_COPY_AND_ASSIGN(WebTestMessageFilter);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_MESSAGE_FILTER_H_
