// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PPAPI_UNITTEST_H_
#define CONTENT_TEST_PPAPI_UNITTEST_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class PepperPluginInstanceImpl;
class PluginModule;

class PpapiUnittest : public testing::Test {
 public:
  PpapiUnittest();
  ~PpapiUnittest() override;

  void SetUp() override;
  void TearDown() override;

  PluginModule* module() const { return module_.get(); }
  PepperPluginInstanceImpl* instance() const { return instance_.get(); }

  // Provides access to the interfaces implemented by the test. The default one
  // implements PPP_INSTANCE.
  virtual const void* GetMockInterface(const char* interface_name) const;

  // Deletes the instance and module to simulate module shutdown.
  void ShutdownModule();

  // Sets the view size of the plugin instance.
  void SetViewSize(int width, int height) const;

 private:
  base::test::TaskEnvironment task_environment_;

  // Note: module must be declared right after |task_environment_| since
  // we want it to get destroyed just before |task_environment_|.
  scoped_refptr<PluginModule> module_;
  scoped_refptr<PepperPluginInstanceImpl> instance_;

  DISALLOW_COPY_AND_ASSIGN(PpapiUnittest);
};

}  // namespace content

#endif  // CONTENT_TEST_PPAPI_UNITTEST_H_
