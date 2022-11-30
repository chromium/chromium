// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_MOCK_EXPORTED_OBJECT_H_
#define DBUS_MOCK_EXPORTED_OBJECT_H_

#include <string>

#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dbus {

// Mock for ExportedObject.
class MockExportedObject : public ExportedObject {
 public:
  MockExportedObject(Bus* bus,
                     const ObjectPath& object_path);

  MOCK_METHOD3(ExportMethodAndBlock,
               bool(const std::string& interface_name,
                    const std::string& method_name,
                    const MethodCallCallback& method_call_callback));
  MOCK_METHOD4(ExportMethod,
               void(const std::string& interface_name,
                    const std::string& method_name,
                    const MethodCallCallback& method_call_callback,
                    OnExportedCallback on_exported_callback));
  MOCK_METHOD2(UnexportMethodAndBlock,
               bool(const std::string& interface_name,
                    const std::string& method_name));
  MOCK_METHOD3(UnexportMethod,
               void(const std::string& interface_name,
                    const std::string& method_name,
                    OnUnexportedCallback on_unexported_callback));
  MOCK_METHOD1(SendSignal, void(Signal* signal));
  MOCK_METHOD0(Unregister, void());

 protected:
  ~MockExportedObject() override;
};

}  // namespace dbus

#endif  // DBUS_MOCK_EXPORTED_OBJECT_H_
