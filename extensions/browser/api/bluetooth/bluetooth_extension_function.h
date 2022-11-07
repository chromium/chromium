// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EXTENSION_FUNCTION_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EXTENSION_FUNCTION_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_function.h"

namespace device {

class BluetoothAdapter;

}  // namespace device

namespace extensions {
namespace api {

// Base class for bluetooth extension functions. This class initializes
// bluetooth adapter and calls (on the UI thread) DoWork() implemented by
// individual bluetooth extension functions.
class BluetoothExtensionFunction : public ExtensionFunction {
 public:
  BluetoothExtensionFunction();

  BluetoothExtensionFunction(const BluetoothExtensionFunction&) = delete;
  BluetoothExtensionFunction& operator=(const BluetoothExtensionFunction&) =
      delete;

 protected:
  ~BluetoothExtensionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Use instead of extension_id() so that this can be run from WebUI.
  std::string GetExtensionId();

  virtual bool CreateParams();

 private:
  void RunOnAdapterReady(scoped_refptr<device::BluetoothAdapter> adapter);

  // Implemented by individual bluetooth extension functions, called
  // automatically on the UI thread once |adapter| has been initialized.
  virtual void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) = 0;
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EXTENSION_FUNCTION_H_
