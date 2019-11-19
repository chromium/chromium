// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_message_filter.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_index_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/web_test_support.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/web_test/blink_test_controller.h"
#include "content/shell/browser/web_test/web_test_browser_context.h"
#include "content/shell/browser/web_test/web_test_content_browser_client.h"
#include "content/shell/browser/web_test/web_test_content_index_provider.h"
#include "content/shell/browser/web_test/web_test_permission_manager.h"
#include "content/shell/common/web_test/web_test_messages.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/test/mock_platform_notification_service.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/quota/quota_manager.h"
#include "url/origin.h"

namespace content {

namespace {

MockPlatformNotificationService* GetMockPlatformNotificationService() {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  auto* service = client->GetPlatformNotificationService(context);

  return static_cast<MockPlatformNotificationService*>(service);
}

WebTestContentIndexProvider* GetWebTestContentIndexProvider() {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  return static_cast<WebTestContentIndexProvider*>(
      context->GetContentIndexProvider());
}

ContentIndexContext* GetContentIndexContext(const url::Origin& origin) {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  auto* storage_partition = BrowserContext::GetStoragePartitionForSite(
      context, origin.GetURL(), /* can_create= */ false);
  return storage_partition->GetContentIndexContext();
}

}  // namespace

WebTestMessageFilter::WebTestMessageFilter(
    int render_process_id,
    storage::DatabaseTracker* database_tracker,
    storage::QuotaManager* quota_manager,
    network::mojom::NetworkContext* network_context)
    : BrowserMessageFilter(WebTestMsgStart),
      render_process_id_(render_process_id),
      database_tracker_(database_tracker),
      quota_manager_(quota_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  network_context->GetCookieManager(
      cookie_manager_.BindNewPipeAndPassReceiver());
}

WebTestMessageFilter::~WebTestMessageFilter() {}

void WebTestMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

scoped_refptr<base::SequencedTaskRunner>
WebTestMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case WebTestHostMsg_ClearAllDatabases::ID:
      return database_tracker_->task_runner();
    case WebTestHostMsg_SimulateWebNotificationClick::ID:
    case WebTestHostMsg_SimulateWebNotificationClose::ID:
    case WebTestHostMsg_SimulateWebContentIndexDelete::ID:
    case WebTestHostMsg_SetPermission::ID:
    case WebTestHostMsg_ResetPermissions::ID:
    case WebTestHostMsg_WebTestRuntimeFlagsChanged::ID:
    case WebTestHostMsg_TestFinishedInSecondaryRenderer::ID:
    case WebTestHostMsg_InitiateCaptureDump::ID:
    case WebTestHostMsg_InspectSecondaryWindow::ID:
    case WebTestHostMsg_DeleteAllCookies::ID:
      return base::CreateSingleThreadTaskRunner({BrowserThread::UI});
  }
  return nullptr;
}

bool WebTestMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebTestMessageFilter, message)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_ReadFileToString, OnReadFileToString)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_RegisterIsolatedFileSystem,
                        OnRegisterIsolatedFileSystem)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_ClearAllDatabases, OnClearAllDatabases)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_SetDatabaseQuota, OnSetDatabaseQuota)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_SimulateWebNotificationClick,
                        OnSimulateWebNotificationClick)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_SimulateWebNotificationClose,
                        OnSimulateWebNotificationClose)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_SimulateWebContentIndexDelete,
                        OnSimulateWebContentIndexDelete)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_DeleteAllCookies, OnDeleteAllCookies)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_SetPermission, OnSetPermission)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_ResetPermissions, OnResetPermissions)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_WebTestRuntimeFlagsChanged,
                        OnWebTestRuntimeFlagsChanged)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_TestFinishedInSecondaryRenderer,
                        OnTestFinishedInSecondaryRenderer)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_InitiateCaptureDump,
                        OnInitiateCaptureDump);
    IPC_MESSAGE_HANDLER(WebTestHostMsg_InspectSecondaryWindow,
                        OnInspectSecondaryWindow)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebTestMessageFilter::OnReadFileToString(const base::FilePath& local_file,
                                              std::string* contents) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ReadFileToString(local_file, contents);
}

void WebTestMessageFilter::OnRegisterIsolatedFileSystem(
    const std::vector<base::FilePath>& absolute_filenames,
    std::string* filesystem_id) {
  storage::IsolatedContext::FileInfoSet files;
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  for (size_t i = 0; i < absolute_filenames.size(); ++i) {
    files.AddPath(absolute_filenames[i], nullptr);
    if (!policy->CanReadFile(render_process_id_, absolute_filenames[i]))
      policy->GrantReadFile(render_process_id_, absolute_filenames[i]);
  }
  *filesystem_id =
      storage::IsolatedContext::GetInstance()->RegisterDraggedFileSystem(files);
  policy->GrantReadFileSystem(render_process_id_, *filesystem_id);
}

void WebTestMessageFilter::OnClearAllDatabases() {
  DCHECK(database_tracker_->task_runner()->RunsTasksInCurrentSequence());
  database_tracker_->DeleteDataModifiedSince(base::Time(),
                                             net::CompletionOnceCallback());
}

void WebTestMessageFilter::OnSetDatabaseQuota(int quota) {
  DCHECK(quota >= 0 || quota == test_runner::kDefaultDatabaseQuota);
  if (quota == test_runner::kDefaultDatabaseQuota) {
    // Reset quota to settings with a zero refresh interval to force
    // QuotaManager to refresh settings immediately.
    storage::QuotaSettings default_settings;
    default_settings.refresh_interval = base::TimeDelta();
    quota_manager_->SetQuotaSettings(default_settings);
  } else {
    quota_manager_->SetQuotaSettings(storage::GetHardCodedSettings(quota));
  }
}

void WebTestMessageFilter::OnSimulateWebNotificationClick(
    const std::string& title,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetMockPlatformNotificationService()->SimulateClick(title, action_index,
                                                      reply);
}

void WebTestMessageFilter::OnSimulateWebNotificationClose(
    const std::string& title,
    bool by_user) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetMockPlatformNotificationService()->SimulateClose(title, by_user);
}

void WebTestMessageFilter::OnSimulateWebContentIndexDelete(
    const std::string& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* provider = GetWebTestContentIndexProvider();

  std::pair<int64_t, url::Origin> registration_data =
      provider->GetRegistrationDataFromId(id);

  auto* context = GetContentIndexContext(registration_data.second);
  context->OnUserDeletedItem(registration_data.first, registration_data.second,
                             id);
}

void WebTestMessageFilter::OnDeleteAllCookies() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cookie_manager_->DeleteCookies(network::mojom::CookieDeletionFilter::New(),
                                 base::BindOnce([](uint32_t) {}));
}

void WebTestMessageFilter::OnSetPermission(
    const std::string& name,
    blink::mojom::PermissionStatus status,
    const GURL& origin,
    const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::PermissionType type;
  if (name == "midi") {
    type = PermissionType::MIDI;
  } else if (name == "midi-sysex") {
    type = PermissionType::MIDI_SYSEX;
  } else if (name == "push-messaging" || name == "notifications") {
    type = PermissionType::NOTIFICATIONS;
  } else if (name == "geolocation") {
    type = PermissionType::GEOLOCATION;
  } else if (name == "protected-media-identifier") {
    type = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
  } else if (name == "background-sync") {
    type = PermissionType::BACKGROUND_SYNC;
  } else if (name == "accessibility-events") {
    type = PermissionType::ACCESSIBILITY_EVENTS;
  } else if (name == "clipboard-read") {
    type = PermissionType::CLIPBOARD_READ;
  } else if (name == "clipboard-write") {
    type = PermissionType::CLIPBOARD_WRITE;
  } else if (name == "payment-handler") {
    type = PermissionType::PAYMENT_HANDLER;
  } else if (name == "accelerometer" || name == "gyroscope" ||
             name == "magnetometer" || name == "ambient-light-sensor") {
    type = PermissionType::SENSORS;
  } else if (name == "background-fetch") {
    type = PermissionType::BACKGROUND_FETCH;
  } else if (name == "periodic-background-sync") {
    type = PermissionType::PERIODIC_BACKGROUND_SYNC;
  } else if (name == "wake-lock-screen") {
    type = PermissionType::WAKE_LOCK_SCREEN;
  } else if (name == "wake-lock-system") {
    type = PermissionType::WAKE_LOCK_SYSTEM;
  } else if (name == "nfc") {
    type = PermissionType::NFC;
  } else {
    NOTREACHED();
    type = PermissionType::NOTIFICATIONS;
  }

  WebTestContentBrowserClient::Get()
      ->GetWebTestBrowserContext()
      ->GetWebTestPermissionManager()
      ->SetPermission(type, status, origin, embedding_origin);
}

void WebTestMessageFilter::OnResetPermissions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebTestContentBrowserClient::Get()
      ->GetWebTestBrowserContext()
      ->GetWebTestPermissionManager()
      ->ResetPermissions();
}

void WebTestMessageFilter::OnWebTestRuntimeFlagsChanged(
    const base::DictionaryValue& changed_web_test_runtime_flags) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get()) {
    BlinkTestController::Get()->OnWebTestRuntimeFlagsChanged(
        render_process_id_, changed_web_test_runtime_flags);
  }
}

void WebTestMessageFilter::OnTestFinishedInSecondaryRenderer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get())
    BlinkTestController::Get()->OnTestFinishedInSecondaryRenderer();
}

void WebTestMessageFilter::OnInitiateCaptureDump(
    bool capture_navigation_history,
    bool capture_pixels) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get()) {
    BlinkTestController::Get()->OnInitiateCaptureDump(
        capture_navigation_history, capture_pixels);
  }
}

void WebTestMessageFilter::OnInspectSecondaryWindow() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get())
    BlinkTestController::Get()->OnInspectSecondaryWindow();
}

}  // namespace content
