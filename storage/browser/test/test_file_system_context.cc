// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/test_file_system_context.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_options.h"

namespace storage {

scoped_refptr<FileSystemContext> CreateFileSystemContextForTesting(
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    const base::FilePath& base_path) {
  std::vector<std::unique_ptr<FileSystemBackend>> additional_providers;
  additional_providers.push_back(std::make_unique<TestFileSystemBackend>(
      base::SingleThreadTaskRunner::GetCurrentDefault().get(), base_path));
  return CreateFileSystemContextWithAdditionalProvidersForTesting(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      std::move(quota_manager_proxy), std::move(additional_providers),
      base_path);
}

scoped_refptr<FileSystemContext>
CreateFileSystemContextWithAdditionalProvidersForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    std::vector<std::unique_ptr<FileSystemBackend>> additional_providers,
    const base::FilePath& base_path) {
  return FileSystemContext::Create(
      std::move(io_task_runner), std::move(file_task_runner),
      ExternalMountPoints::CreateRefCounted(),
      base::MakeRefCounted<MockSpecialStoragePolicy>(),
      std::move(quota_manager_proxy), std::move(additional_providers),
      std::vector<URLRequestAutoMountHandler>(), base_path,
      CreateAllowFileAccessOptions());
}

scoped_refptr<FileSystemContext>
CreateFileSystemContextWithAutoMountersForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    std::vector<std::unique_ptr<FileSystemBackend>> additional_providers,
    const std::vector<URLRequestAutoMountHandler>& auto_mounters,
    const base::FilePath& base_path) {
  return FileSystemContext::Create(
      std::move(io_task_runner), std::move(file_task_runner),
      ExternalMountPoints::CreateRefCounted(),
      base::MakeRefCounted<MockSpecialStoragePolicy>(),
      std::move(quota_manager_proxy), std::move(additional_providers),
      auto_mounters, base_path, CreateAllowFileAccessOptions());
}

scoped_refptr<FileSystemContext> CreateIncognitoFileSystemContextForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    const base::FilePath& base_path) {
  std::vector<std::unique_ptr<FileSystemBackend>> additional_providers;
  additional_providers.push_back(std::make_unique<TestFileSystemBackend>(
      base::SingleThreadTaskRunner::GetCurrentDefault().get(), base_path));
  return CreateIncognitoFileSystemContextWithAdditionalProvidersForTesting(
      std::move(io_task_runner), std::move(file_task_runner),
      std::move(quota_manager_proxy), std::move(additional_providers),
      base_path);
}

scoped_refptr<FileSystemContext>
CreateIncognitoFileSystemContextWithAdditionalProvidersForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    std::vector<std::unique_ptr<FileSystemBackend>> additional_providers,
    const base::FilePath& base_path) {
  return FileSystemContext::Create(
      std::move(io_task_runner), std::move(file_task_runner),
      ExternalMountPoints::CreateRefCounted(),
      base::MakeRefCounted<MockSpecialStoragePolicy>(),
      std::move(quota_manager_proxy), std::move(additional_providers),
      std::vector<URLRequestAutoMountHandler>(), base_path,
      CreateIncognitoFileSystemOptions());
}

}  // namespace storage
