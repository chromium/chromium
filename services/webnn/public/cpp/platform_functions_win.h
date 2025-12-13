// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_PLATFORM_FUNCTIONS_WIN_H_
#define SERVICES_WEBNN_PUBLIC_CPP_PLATFORM_FUNCTIONS_WIN_H_

#include <windows.h>

#include <appmodel.h>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/strings/cstring_view.h"

namespace webnn {

// Creates and adds the package dependency with the lifetime of a process.
// Returns the package path if successful, or an empty path on failure.
base::FilePath COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    InitializePackageDependencyForProcess(
        base::wcstring_view package_family_name,
        PACKAGE_VERSION min_version);

// Creates the package dependency with the lifetime of the provided file path.
// Returns the dependency ID if successful, or an empty string on failure.
std::wstring COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    TryCreatePackageDependencyForFilePath(
        base::wcstring_view package_family_name,
        PACKAGE_VERSION min_version,
        const base::FilePath& file_path);

// Creates the package dependency with the lifetime of a process. Returns the
// dependency ID if successful, or an empty string on failure.
std::wstring COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    TryCreatePackageDependencyForProcess(
        base::wcstring_view package_family_name,
        PACKAGE_VERSION min_version);

// Adds the package dependency using the provided dependency ID. Returns the
// package path if successful, or an empty path on failure.
base::FilePath COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    AddPackageDependency(base::wcstring_view dependency_id);

// Deletes the package dependency using the provided dependency ID.
bool COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    DeletePackageDependency(base::wcstring_view dependency_id);

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_PLATFORM_FUNCTIONS_WIN_H_
