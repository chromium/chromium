// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SOURCE_LOCATION_H_
#define REMOTING_BASE_SOURCE_LOCATION_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/location.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace remoting {

namespace mojom {
class SourceLocationDataView;
}  // namespace mojom

// SourceLocation represents certain information about the source code, such as
// file names, line numbers, and function names. This is similar to
// base::Location, but it has the following differences to allow it to be passed
// across the process boundary (via Mojo IPC):
//
// 1. SourceLocation has an optional backing store for the function name and
//    the file name, which base::Location doesn't have. Since we can't pass
//    addresses in the data segment across the process boundary, the backing
//    store is critical for reconstructing the SourceLocation object from
//    strings that get passed through IPC.
//    If the SourceLocation stays in the same process, the backing store will
//    not be created and the memory footprint is roughly the same as
//    base::Location.
//
// 2. SourceLocation does not store the program counter, since remoting does not
//    need this info, and in most cases we can't pass a code segment address
//    across the process boundary.
//
// SourceLocation can be implicitly converted from base::Location, so you can
// create it with `FROM_HERE`. It can't be converted back to base::Location
// because of the differences stated above.
class SourceLocation final {
 public:
  SourceLocation();
  SourceLocation(SourceLocation&&);
  SourceLocation(const SourceLocation&);

  // Allow creation of SourceLocation with FROM_HERE.
  // NOLINTNEXTLINE(google-explicit-constructor)
  SourceLocation(const base::Location& location);
  ~SourceLocation();

  SourceLocation& operator=(SourceLocation&&);
  SourceLocation& operator=(const SourceLocation&);

  bool operator==(const SourceLocation& other) const;

  // Returns true if there is source code location info. If this is false, the
  // location object is default-initialized.
  bool is_null() const { return !function_name_ || !file_name_; }

  // Will be nullptr for default initialized Location objects.
  const char* function_name() const { return function_name_; }

  // Will be nullptr for default initialized Location objects.
  const char* file_name() const { return file_name_; }

  // Will be -1 for default initialized Location objects.
  int line_number() const { return line_number_; }

  // Converts to the most user-readable form possible.
  std::string ToString() const;

  bool HasBackingStoreForTesting() const;

  static SourceLocation CreateWithBackingStoreForTesting(
      std::optional<std::string_view> function_name,
      std::optional<std::string_view> file_name,
      int line_number);

 private:
  friend struct mojo::StructTraits<remoting::mojom::SourceLocationDataView,
                                   ::remoting::SourceLocation>;

  struct BackingStore;

  void InitializeWithBackingStore(std::optional<std::string_view> function_name,
                                  std::optional<std::string_view> file_name,
                                  int line_number);

  const char* function_name_ = nullptr;
  const char* file_name_ = nullptr;
  int line_number_ = -1;

  std::unique_ptr<BackingStore> backing_store_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_SOURCE_LOCATION_H_
