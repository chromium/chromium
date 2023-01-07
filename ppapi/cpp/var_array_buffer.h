// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VAR_ARRAY_BUFFER_H_
#define PPAPI_CPP_VAR_ARRAY_BUFFER_H_

#include <stdint.h>

#include "ppapi/cpp/var.h"

/// @file
/// This file defines the API for interacting with a JavaScript ArrayBuffer.

namespace pp {

/// <code>VarArrayBuffer</code> provides a way to interact with JavaScript
/// ArrayBuffers, which represent a contiguous sequence of bytes. Note that
/// these vars are not part of the embedding page's DOM, and can only be
/// shared with JavaScript using the <code>PostMessage</code> and
/// <code>HandleMessage</code> functions of <code>Instance</code>.
class VarArrayBuffer : public Var {
 public:
  /// The default constructor constructs a <code>VarArrayBuffer</code> which is
  /// 0 byte long.
  VarArrayBuffer();

  /// Construct a <code>VarArrayBuffer</code> given a var for which
  /// is_array_buffer() is true. This will refer to the same
  /// <code>ArrayBuffer</code> as var, but allows you to access methods
  /// specific to <code>VarArrayBuffer</code>.
  ///
  /// @param[in] var An <code>ArrayBuffer</code> var.
  explicit VarArrayBuffer(const Var& var);

  /// Construct a new <code>VarArrayBuffer</code> which is
  /// <code>size_in_bytes</code> bytes long and initialized to zero.
  ///
  /// @param[in] size_in_bytes The size of the constructed
  /// <code>ArrayBuffer</code> in bytes.
  explicit VarArrayBuffer(uint32_t size_in_bytes);

  /// Copy constructor.
  VarArrayBuffer(const VarArrayBuffer& buffer) : Var(buffer) {}

  virtual ~VarArrayBuffer() {}

  /// This function assigns one <code>VarArrayBuffer</code> to another
  /// <code>VarArrayBuffer</code>.
  ///
  /// @param[in] other The <code>VarArrayBuffer</code> to be assigned.
  ///
  /// @return The resulting <code>VarArrayBuffer</code>.
  VarArrayBuffer& operator=(const VarArrayBuffer& other);

  /// This function assigns one <code>VarArrayBuffer</code> to another
  /// <code>VarArrayBuffer</code>. A Var's assignment operator is overloaded
  /// here so that we can check for assigning a non-ArrayBuffer var to a
  /// <code>VarArrayBuffer</code>.
  ///
  /// @param[in] other The <code>VarArrayBuffer</code> to be assigned.
  ///
  /// @return The resulting <code>VarArrayBuffer</code> (as a Var&).
  virtual Var& operator=(const Var& other);

  /// ByteLength() retrieves the length of the <code>VarArrayBuffer</code> in
  /// bytes.
  ///
  /// @return The length of the <code>VarArrayBuffer</code> in bytes.
  uint32_t ByteLength() const;

  /// Map() maps the <code>ArrayBuffer</code> in to the module's address space
  /// and returns a pointer to the beginning of the internal buffer for
  /// this <code>ArrayBuffer</code>. ArrayBuffers are copied when transmitted,
  /// so changes to the underlying memory are not automatically available to
  /// the embedding page.
  ///
  /// Note that calling Map() can be a relatively expensive operation. Use care
  /// when calling it in performance-critical code. For example, you should call
  /// it only once when looping over an <code>ArrayBuffer</code>.
  ///
  /// <strong>Example:</strong>
  ///
  /// @code
  ///   char* data = static_cast<char*>(array_buffer_var.Map());
  ///   uint32_t byte_length = array_buffer_var.ByteLength();
  ///   for (uint32_t i = 0; i < byte_length; ++i)
  ///     data[i] = 'A';
  /// @endcode
  ///
  /// @return A pointer to the internal buffer for this
  /// <code>ArrayBuffer</code>.
  void* Map();

  /// Unmap() unmaps this <code>ArrayBuffer</code> var from the module address
  /// space. Use this if you want to save memory but might want to call Map()
  /// to map the buffer again later.
  void Unmap();

 private:
  void ConstructWithSize(uint32_t size_in_bytes);
};

}  // namespace pp

#endif  // PPAPI_CPP_VAR_ARRAY_BUFFER_H_
