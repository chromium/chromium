// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PROCESS_SELECTION_USER_DATA_H_
#define CONTENT_PUBLIC_BROWSER_PROCESS_SELECTION_USER_DATA_H_

#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"

namespace content {

// This class is a container for embedder-specific data that is available
// during the renderer process selection phase of a navigation. An instance of
// this class is created for each NavigationRequest and is accessible from
// ProcessSelectionDeferringConditions. Embedders can define their own data
// classes that inherit from ProcessSelectionUserData::Data and attach them to
// this object.
//
// Example:
//
//   // --- in foo_process_selection_deferring_condition.h ---
//   class FooData : public content::ProcessSelectionUserData::Data<FooData> {
//    public:
//     // other public members
//
//    private:
//     friend ProcessSelectionUserData::Data;
//     PROCESS_SELECTION_USER_DATA_KEY_DECL();
//   };
//
//   class FooProcessSelectionDeferringCondition
//       : public content::ProcessSelectionDeferringCondition {
//     // ...
//   };
//
//   // --- in foo_process_selection_deferring_condition.cc ---
//   PROCESS_SELECTION_USER_DATA_KEY_IMPL(FooData);
//
//   Result FooProcessSelectionDeferringCondition::OnWillSelectFinalProcess()
//   override {
//     content::ProcessSelectionUserData& user_data =
//         GetProcessSelectionUserData();
//     user_data.SetUserData(FooData::UserDataKey(),
//                           std::make_unique<FooData>(kSomeData));
//     return Result::kProceed;
//   }
//
//  To access the data from the process selection logic:
//
//   if (url_info().process_selection_user_data) {
//     const FooData* foo_data =
//         FooData::FromProcessSelectionUserData(
//             url_info().process_selection_user_data);
//    // ...
//   }
class CONTENT_EXPORT ProcessSelectionUserData : public base::SupportsUserData {
 public:
  ProcessSelectionUserData();
  ~ProcessSelectionUserData() override;

  base::SafeRef<ProcessSelectionUserData> GetSafeRef();

  // A base class for classes attached to ProcessSelectionUserData.
  template <typename T>
  class Data : public base::SupportsUserData::Data {
   public:
    // Returns the instance of type `T` from the `data` this is provided as an
    // argument. Returns a nullptr if no instance of `T` is found on `data`.
    static const T* FromProcessSelectionUserData(
        const std::optional<base::SafeRef<ProcessSelectionUserData>>& data) {
      if (!data) {
        return nullptr;
      }
      const ProcessSelectionUserData* user_data = &**data;
      return static_cast<const T*>(user_data->GetUserData(UserDataKey()));
    }
    static const T* FromProcessSelectionUserData(
        const ProcessSelectionUserData& data) {
      return static_cast<const T*>(data.GetUserData(UserDataKey()));
    }

    static const void* UserDataKey() { return &T::kUserDataKey; }
  };

 private:
  base::WeakPtrFactory<ProcessSelectionUserData> weak_ptr_factory_{this};
};

// Subclasses of ProcessSelectionUserData::Data call this macro in the header.
#define PROCESS_SELECTION_USER_DATA_KEY_DECL() static const int kUserDataKey = 0

// Subclasses of ProcessSelectionUserData::Data call this macro in the cc file.
#define PROCESS_SELECTION_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PROCESS_SELECTION_USER_DATA_H_
