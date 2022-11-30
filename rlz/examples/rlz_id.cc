// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/machine_id.h"

int main() {
  std::string machine_id;
  if (!rlz_lib::GetMachineId(&machine_id))
    return 1;

  printf("%s\n", machine_id.c_str());
}
