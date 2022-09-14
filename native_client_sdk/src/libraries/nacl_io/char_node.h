// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_CHAR_NODE_H_
#define LIBRARIES_NACL_IO_CHAR_NODE_H_

#include "nacl_io/node.h"

namespace nacl_io {

class CharNode : public Node {
 public:
  explicit CharNode(Filesystem* filesystem) : Node(filesystem) {
    SetType(S_IFCHR);
  }
};
}

#endif  // LIBRARIES_NACL_IO_CHAR_NODE_H_
