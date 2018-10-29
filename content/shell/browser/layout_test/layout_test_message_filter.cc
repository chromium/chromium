// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_message_filter.h"

#include <stddef.h>

#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/browser/layout_test/blink_test_controller.h"
#include "content/shell/browser/layout_test/layout_test_browser_context.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/layout_test/layout_test_permission_manager.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "content/shell/common/layout_test/layout_test_messages.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/test/mock_platform_notification_service.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/browser/quota/quota_manager.h"

namespace content {

LayoutTestMessageFilter::LayoutTestMessageFilter(
    int render_process_id,
    storage::DatabaseTracker* database_tracker,
    storage::QuotaManager* quota_manager,
    net::URLRequestContextGetter* request_context_getter,
    network::mojom::NetworkContext* network_context)
    : BrowserMessageFilter(LayoutTestMsgStart),
      render_process_id_(render_process_id),
      database_tracker_(database_tracker),
      quota_manager_(quota_manager),
      request_context_getter_(request_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (network_context)
    network_context->GetCookieManager(mojo::MakeRequest(&cookie_manager_));
}

LayoutTestMessageFilter::~LayoutTestMessageFilter() {
}

void LayoutTestMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

base::TaskRunner* LayoutTestMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case LayoutTestHostMsg_ClearAllDatabases::ID:
      return database_tracker_->task_runner();
    case LayoutTestHostMsg_SimulateWebNotificationClick::ID:
    case LayoutTestHostMsg_SimulateWebNotificationClose::ID:
    case LayoutTestHostMsg_SetPermission::ID:
    case LayoutTestHostMsg_ResetPermissions::ID:
    case LayoutTestHostMsg_LayoutTestRuntimeFlagsChanged::ID:
    case LayoutTestHostMsg_TestFinishedInSecondaryRenderer::ID:
    case LayoutTestHostMsg_InitiateCaptureDump::ID:
    case LayoutTestHostMsg_InspectSecondaryWindow::ID:
    case LayoutTestHostMsg_DeleteAllCookiesForNetworkService::ID:
      return base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI})
          .get();
  }
  return nullptr;
}

bool LayoutTestMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(LayoutTestMessageFilter, message)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_ReadFileToString, OnReadFileToString)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_RegisterIsolatedFileSystem,
                        OnRegisterIsolatedFileSystem)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_ClearAllDatabases,
                        OnClearAllDatabases)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_SetDatabaseQuota, OnSetDatabaseQuota)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_SimulateWebNotificationClick,
                        OnSimulateWebNotificationClick)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_SimulateWebNotificationClose,
                        OnSimulateWebNotificationClose)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_DeleteAllCookies, OnDeleteAllCookies)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_DeleteAllCookiesForNetworkService,
                        OnDeleteAllCookiesForNetworkService)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_SetPermission, OnSetPermission)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_ResetPermissions, OnResetPermissions)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_LayoutTestRuntimeFlagsChanged,
                        OnLayoutTestRuntimeFlagsChanged)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_TestFinishedInSecondaryRenderer,
                        OnTestFinishedInSecondaryRenderer)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_InitiateCaptureDump,
                        OnInitiateCaptureDump);
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_InspectSecondaryWindow,
                        OnInspectSecondaryWindow)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void LayoutTestMessageFilter::OnReadFileToString(
    const base::FilePath& local_file, std::string* contents) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ReadFileToString(local_file, contents);
}

void LayoutTestMessageFilter::OnRegisterIsolatedFileSystem(
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

void LayoutTestMessageFilter::OnClearAllDatabases() {
  DCHECK(database_tracker_->task_runner()->RunsTasksInCurrentSequence());
  database_tracker_->DeleteDataModifiedSince(base::Time(),
                                             net::CompletionCallback());
}

void LayoutTestMessageFilter::OnSetDatabaseQuota(int quota) {
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

void LayoutTestMessageFilter::OnSimulateWebNotificationClick(
    const std::string& title,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MockPlatformNotificationService* platform_notification_service =
      static_cast<MockPlatformNotificationService*>(
          LayoutTestContentBrowserClient::Get()
              ->GetPlatformNotificationService());

  platform_notification_service->SimulateClick(title, action_index, reply);
}

void LayoutTestMessageFilter::OnSimulateWebNotificationClose(
    const std::string& title, bool by_user) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MockPlatformNotificationService* platform_notification_service =
      static_cast<MockPlatformNotificationService*>(
          LayoutTestContentBrowserClient::Get()
              ->GetPlatformNotificationService());

  platform_notification_service->SimulateClose(title, by_user);
}

void LayoutTestMessageFilter::OnDeleteAllCookies() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContext* context =
      request_context_getter_->GetURLRequestContext();
  if (context)
    context->cookie_store()->DeleteAllAsync(net::CookieStore::DeleteCallback());
}

void LayoutTestMessageFilter::OnDeleteAllCookiesForNetworkService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (cookie_manager_) {
    cookie_manager_->DeleteCookies(network::mojom::CookieDeletionFilter::New(),
                                   base::BindOnce([](uint32_t) {}));
  }
}

void LayoutTestMessageFilter::OnSetPermission(
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
  } else {
    NOTREACHED();
    type = PermissionType::NOTIFICATIONS;
  }

  LayoutTestContentBrowserClient::Get()
      ->GetLayoutTestBrowserContext()
      ->GetLayoutTestPermissionManager()
      ->SetPermission(type, status, origin, embedding_origin);
}

void LayoutTestMessageFilter::OnResetPermissions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  LayoutTestContentBrowserClient::Get()
      ->GetLayoutTestBrowserContext()
      ->GetLayoutTestPermissionManager()
      ->ResetPermissions();
}

void LayoutTestMessageFilter::OnLayoutTestRuntimeFlagsChanged(
    const base::DictionaryValue& changed_layout_test_runtime_flags) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get()) {
    BlinkTestController::Get()->OnLayoutTestRuntimeFlagsChanged(
        render_process_id_, changed_layout_test_runtime_flags);
  }
}

void LayoutTestMessageFilter::OnTestFinishedInSecondaryRenderer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get())
    BlinkTestController::Get()->OnTestFinishedInSecondaryRenderer();
}

void LayoutTestMessageFilter::OnInitiateCaptureDump(
    bool capture_navigation_history,
    bool capture_pixels) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get()) {
    BlinkTestController::Get()->OnInitiateCaptureDump(
        capture_navigation_history, capture_pixels);
  }
}

void LayoutTestMessageFilter::OnInspectSecondaryWindow() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get())
    BlinkTestController::Get()->OnInspectSecondaryWindow();
}

}  // namespace content
