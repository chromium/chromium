// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_LOCK_SCREEN_DATA_API_H_
#define EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_LOCK_SCREEN_DATA_API_H_

#include <memory>
#include <vector>

#include "extensions/browser/extension_function.h"

namespace extensions {

namespace lock_screen_data {
enum class OperationResult;
class DataItem;
}  // namespace lock_screen_data

class LockScreenDataCreateFunction : public ExtensionFunction {
 public:
  LockScreenDataCreateFunction();

  LockScreenDataCreateFunction(const LockScreenDataCreateFunction&) = delete;
  LockScreenDataCreateFunction& operator=(const LockScreenDataCreateFunction&) =
      delete;

 private:
  ~LockScreenDataCreateFunction() override;

  ResponseAction Run() override;

  void OnDone(lock_screen_data::OperationResult result,
              const lock_screen_data::DataItem* item);

  DECLARE_EXTENSION_FUNCTION("lockScreen.data.create", LOCKSCREENDATA_CREATE)
};

class LockScreenDataGetAllFunction : public ExtensionFunction {
 public:
  LockScreenDataGetAllFunction();

  LockScreenDataGetAllFunction(const LockScreenDataGetAllFunction&) = delete;
  LockScreenDataGetAllFunction& operator=(const LockScreenDataGetAllFunction&) =
      delete;

 private:
  ~LockScreenDataGetAllFunction() override;

  ResponseAction Run() override;

  void OnDone(const std::vector<const lock_screen_data::DataItem*>& items);

  DECLARE_EXTENSION_FUNCTION("lockScreen.data.getAll", LOCKSCREENDATA_GETALL)
};

class LockScreenDataGetContentFunction : public ExtensionFunction {
 public:
  LockScreenDataGetContentFunction();

  LockScreenDataGetContentFunction(const LockScreenDataGetContentFunction&) =
      delete;
  LockScreenDataGetContentFunction& operator=(
      const LockScreenDataGetContentFunction&) = delete;

 private:
  ~LockScreenDataGetContentFunction() override;

  ResponseAction Run() override;

  void OnDone(lock_screen_data::OperationResult result,
              std::unique_ptr<std::vector<char>> data);

  DECLARE_EXTENSION_FUNCTION("lockScreen.data.getContent",
                             LOCKSCREENDATA_GETCONTENT)
};

class LockScreenDataSetContentFunction : public ExtensionFunction {
 public:
  LockScreenDataSetContentFunction();

  LockScreenDataSetContentFunction(const LockScreenDataSetContentFunction&) =
      delete;
  LockScreenDataSetContentFunction& operator=(
      const LockScreenDataSetContentFunction&) = delete;

 private:
  ~LockScreenDataSetContentFunction() override;

  ResponseAction Run() override;

  void OnDone(lock_screen_data::OperationResult result);

  DECLARE_EXTENSION_FUNCTION("lockScreen.data.setContent",
                             LOCKSCREENDATA_SETCONTENT)
};

class LockScreenDataDeleteFunction : public ExtensionFunction {
 public:
  LockScreenDataDeleteFunction();

  LockScreenDataDeleteFunction(const LockScreenDataDeleteFunction&) = delete;
  LockScreenDataDeleteFunction& operator=(const LockScreenDataDeleteFunction&) =
      delete;

 private:
  ~LockScreenDataDeleteFunction() override;

  ResponseAction Run() override;

  void OnDone(lock_screen_data::OperationResult result);

  DECLARE_EXTENSION_FUNCTION("lockScreen.data.delete", LOCKSCREENDATA_DELETE)
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_LOCK_SCREEN_DATA_API_H_
