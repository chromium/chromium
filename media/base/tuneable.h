// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TUNEABLE_H_
#define MEDIA_BASE_TUNEABLE_H_

#include "base/unguessable_token.h"
#include "media/base/media_export.h"

namespace media {

// One tuneable value.  Each Tuneable has:
//  name - Used to set the finch parameter name.
//  value - Current runtime value.  All Tuneable instances with the same name
//          will return the same value.  This value will be a constant over the
//          lifetime of the process as well.
//  min / default / max values - hardcoded range and default for this tuneable.
//
// Via finch, one may enable randomization of the Tuneables, such that a value
// will be chosen at random between a finch-provided minimum and maximum.  This
// minimum and maximum will be constrained by the hardcoded one provided during
// construction.  Different Tuneable instances for the same name are still
// guaranteed to be equal, as described above.
template <typename T>
class MEDIA_EXPORT Tuneable {
 public:
  Tuneable(const char* name, T minimum_value, T default_value, T maximum_value);
  Tuneable(const Tuneable&) = delete;
  Tuneable(Tuneable&&) = delete;
  ~Tuneable();

  Tuneable& operator=(const Tuneable&) = delete;
  Tuneable& operator=(Tuneable&&) = delete;

  T value() const { return t_; }

  void set_for_testing(T value) { t_ = value; }

 private:
  T t_;
};

// Set the random number seed that the tuneable will use to choose its value.
// Must be called before the first tuneable is constructed.
void MEDIA_EXPORT SetRandomSeedForTuneables(const base::UnguessableToken& seed);

}  // namespace media

#endif  // MEDIA_BASE_TUNEABLE_H_
