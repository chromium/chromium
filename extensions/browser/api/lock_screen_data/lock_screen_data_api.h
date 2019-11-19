// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_LOCK_SCREEN_DATA_API_H_
#define EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_LOCK_SCREEN_DATA_API_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace lock_screen_data {
enum class OperationResult;
class DataItem;
}  // namespace lock_screen_data

class LockScreenDataCreateFunction : public ExtensionFunction {
 public:
  LockScreenDataCreateFunction();

 private:
  ~LockScreenDataCreateFunction() override;

  ResponseAction Run() override;

  void OnDone(lock_screen_data::OperationResult result,
              const lock_screen_data::DataItem* item);

  DECLARE_EXTENSION_FUNCTION("lockScreen.data.create", LOCKSCREENDATA_CREATE)
  DISALLOW_COPY_AND_ASSIGN(LockScreenDataCreateFunction);
};

class LockScreenDataGetAllFunction : public ExtensionFunction {
 public:
  LockScreenDataGetAllFunction();

 private:
  ~LockScreenDataGetAllFunction() override;

  ResponseAction Run() override;

  void OnDone(const std::vector<const lock_screen_data::DataItem*>& items);

  DECLARE_EXTENSION_FUNCTION("lockScreen.data.getAll", LOCKSCREENDATA_GETALL)
  DISALLOW_COPY_AND_ASSIGN(LockScreenDataGetAllFunction);
};

class LockScreenDataGetContentFunction : public ExtensionFunction {
 public:
  LockScreenDataGetContentFunction();

 private:
  ~LockScreenDataGetContentFunction() override;

  ResponseAction Run() override;

  void OnDone(lock_screen_data::OperationResult result,
              std::unique_ptr<std::vector<char>> data);

  DECLARE_EXTENSION_FUNCTION("lockScreen.data.getContent",
                             LOCKSCREENDATA_GETCONTENT)
  DISALLOW_COPY_AND_ASSIGN(LockScreenDataGetContentFunction);
};

class LockScreenDataSetContentFunction : public ExtensionFunction {
 public:
  LockScreenDataSetContentFunction();

 private:
  ~LockScreenDataSetContentFunction() override;

  ResponseAction Run() override;

  void OnDone(lock_screen_data::OperationResult result);

  DECLARE_EXTENSION_FUNCTION("lockScreen.data.setContent",
                             LOCKSCREENDATA_SETCONTENT)
  DISALLOW_COPY_AND_ASSIGN(LockScreenDataSetContentFunction);
};

class LockScreenDataDeleteFunction : public ExtensionFunction {
 public:
  LockScreenDataDeleteFunction();

 private:
  ~LockScreenDataDeleteFunction() override;

  ResponseAction Run() override;

  void OnDone(lock_screen_data::OperationResult result);

  DECLARE_EXTENSION_FUNCTION("lockScreen.data.delete", LOCKSCREENDATA_DELETE)

  DISALLOW_COPY_AND_ASSIGN(LockScreenDataDeleteFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_LOCK_SCREEN_DATA_API_H_
