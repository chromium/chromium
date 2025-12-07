// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASE_LOGGABLE_H_
#define REMOTING_HOST_BASE_LOGGABLE_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/location.h"
#include "base/types/expected.h"

namespace remoting {

// A class designed to be used as the error type for a base::expected value when
// a more structured error type is not useful or practical.
//
// Instances contain both an initial error message (e.g., "Input/output error")
// along with any additional context added by higher levels (e.g., "While
// reading configuration file: foo.conf").
//
// The resulting chain can be printed or logged using the stream insertion
// operator. E.g.,
//
//     LOG(ERROR) << loggable;
class Loggable {
 public:
  // Constructs an empty loggable only suitable as an assignment target.
  Loggable();
  // Constructs a loggable with the given code location and message.
  Loggable(base::Location from_here, std::string message);

  // Copyable and movable.
  Loggable(const Loggable&);
  Loggable(Loggable&&);
  Loggable& operator=(const Loggable&);
  Loggable& operator=(Loggable&&);

  ~Loggable();

  // Adds a context entry to this.
  void AddContext(base::Location from_here, std::string context);
  // Convenience method to add a context entry and wrap this as an unexpected
  // value. Allows, e.g.,
  //
  //     return std::move(result.error()).UnexpectedWithContext(
  //         FROM_HERE, "While doing foo");
  base::unexpected<Loggable> UnexpectedWithContext(base::Location from_here,
                                                   std::string context) &&;

  std::string ToString() const;

  friend std::ostream& operator<<(std::ostream&, const Loggable&);

 private:
  // Storing the message and context out-of-line allows Loggable to be the size
  // of a single pointer (keeping base::expected instances compact) at the
  // expense of an additional indirection in the error case.
  class Inner;
  std::unique_ptr<Inner> inner_;
};

std::ostream& operator<<(std::ostream&, const Loggable&);

}  // namespace remoting

#endif  // REMOTING_HOST_BASE_LOGGABLE_H_
