// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_USER_DATA_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_USER_DATA_H_

#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"

// A base class for classes attached to, and scoped to, the lifetime of a user
// data container (e.g. OverlayRequest, OverlayResponse).
//
// --- in data.h ---
// class Data : public OverlayUserData<Data> {
//  public:
//   ~Data() override;
//   // ... more public stuff here ...
//  private:
//   friend class OverlayUserData<Data>;
//   explicit Data( \* ANY ARGUMENT LIST SUPPORTED *\);
//   // ... more private stuff here ...
// };
template <class DataType>
class OverlayUserData : public base::SupportsUserData::Data {
 public:
  // Creates an OverlayUserData of type DataType and adds it to `user_data`
  // under its key.  The DataType instance is constructed using the arguments
  // passed after the key to this function.  If a DataType instance already
  // exists in `user_data`, no new object is created.  For example, if the
  // constructor for an OverlayUserData of type StringData takes a string, one
  // can be created using:
  //
  // StringData::CreateForUserData(user_data, "string");
  template <typename... Args>
  static void CreateForUserData(base::SupportsUserData* user_data,
                                Args&&... args) {
    if (!FromUserData(user_data)) {
      std::unique_ptr<DataType> data =
          base::WrapUnique(new DataType(std::forward<Args>(args)...));
      data->CreateAuxiliaryData(user_data);
      user_data->SetUserData(UserDataKey(), std::move(data));
    }
  }

  // Retrieves the instance of type DataType that was attached to the specified
  // user data container and returns it. If no instance of the type was
  // attached, returns nullptr.
  static DataType* FromUserData(base::SupportsUserData* user_data) {
    return static_cast<DataType*>(user_data->GetUserData(UserDataKey()));
  }
  static const DataType* FromUserData(const base::SupportsUserData* user_data) {
    return static_cast<const DataType*>(user_data->GetUserData(UserDataKey()));
  }

 private:
  // The key under which to store the user data.
  static inline const void* UserDataKey() {
    static const int kId = 0;
    return &kId;
  }

  // Adds auxilliary OverlayUserData to `data`.  Used to allow multiple
  // OverlayUserData templates to share common functionality in a separate data
  // stored in `user_data`.
  virtual void CreateAuxiliaryData(base::SupportsUserData* user_data) {}
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_USER_DATA_H_
