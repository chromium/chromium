// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a utility executable used for generating hashes for pepper
// interfaces for inclusion in tools/metrics/histograms/histograms.xml. Every
// interface-version pair must have a corresponding entry in the enum there.
//
// The hashing logic here must match the hashing logic at
// ppapi/proxy/interface_list.cc.
//
// This utility can be used to generate a sorted list of hashes for all current
// PPB* interfaces by running a script to generate the interface names, e.g.
// $ grep -r "PPB_" ppapi/c | grep -o "\".*;[0-9]*\.[0-9]*\"" | tr '\n' ' '
// and then invoking pepper_hash_for_uma on the list. The sorted output hashes
// can be compared to tools/metrics/histograms/histograms.xml to determine if
// any interfaces have been left out.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "base/hash/hash.h"
#include "base/macros.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <interface1> <interface2> <...>\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "Prints hashes for interface names.\n");
    fprintf(stderr, "Example: %s \"PPB_Var;1.1\" \"PPB_FileIO;1.2\"\n",
            argv[0]);
    return 1;
  }
  std::vector<std::pair<uint32_t, char*>> hashes;
  for (int i = 1; i < argc; i++) {
    uint32_t data = base::Hash(argv[i], strlen(argv[i]));

    // Strip off the signed bit because UMA doesn't support negative values,
    // but takes a signed int as input.
    int hash = static_cast<int>(data & 0x7fffffff);
    hashes.push_back(std::make_pair(hash, argv[i]));
  }
  std::sort(hashes.begin(), hashes.end());
  for (const auto& hash : hashes) {
    printf("<int value=\"%d\" label=\"%s\"/>\n", hash.first, hash.second);
  }

  return 0;
}
