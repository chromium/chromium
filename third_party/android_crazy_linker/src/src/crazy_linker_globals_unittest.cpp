// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_globals.h"

#include <gtest/gtest.h>

#include "crazy_linker_system_mock.h"

namespace crazy {

TEST(Globals, Get) {
  SystemMock sys;
  Globals* globals = Globals::Get();
  ASSERT_TRUE(globals);
  ASSERT_TRUE(globals->libraries());
  ASSERT_TRUE(globals->search_path_list());
  ASSERT_TRUE(globals->rdebug());
}

}  // namespace crazy
