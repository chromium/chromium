// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_PRELOADED_STATE_GENERATOR_H_
#define NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_PRELOADED_STATE_GENERATOR_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/time/time.h"
#include "net/tools/huffman_trie/trie/trie_writer.h"
#include "net/tools/transport_security_state_generator/pinset.h"
#include "net/tools/transport_security_state_generator/pinsets.h"
#include "net/tools/transport_security_state_generator/transport_security_state_entry.h"

namespace net::transport_security_state {

// PreloadedStateGenerator generates C++ code that contains the preloaded
// entries in a way the Chromium code understands. The code that reads the
// output can be found in net/http/transport_security_state.cc. The output gets
// compiled into the binary.
class PreloadedStateGenerator {
 public:
  PreloadedStateGenerator();
  ~PreloadedStateGenerator();

  // Returns the generated C++ code on success and the empty string on failure.
  std::string Generate(const std::string& preload_template,
                       const TransportSecurityStateEntries& entries,
                       const Pinsets& pinsets,
                       const base::Time& timestamp);

 private:
  void ProcessSPKIHashes(const Pinsets& pinset, std::string* tpl);
  void ProcessPinsets(const Pinsets& pinset,
                      NameIDMap* pinset_map,
                      std::string* tpl);
};

}  // namespace net::transport_security_state

#endif  // NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_PRELOADED_STATE_GENERATOR_H_
