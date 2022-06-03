// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_COURGETTE_H_
#define COURGETTE_COURGETTE_H_

#include <stddef.h>   // Required to define size_t on GCC

#include "base/files/file.h"
#include "base/files/file_path.h"

namespace courgette {

// Status codes for Courgette APIs.
//
// Client code should only rely on the distintion between C_OK and the other
// status codes.
//
enum Status {
  C_OK = 1,                       // Successful operation.

  C_GENERAL_ERROR = 2,            // Error other than listed below.

  C_READ_OPEN_ERROR = 3,          // Could not open input file for reading.
  C_READ_ERROR = 4,               // Could not read from opened input file.

  C_WRITE_OPEN_ERROR = 3,         // Could not open output file for writing.
  C_WRITE_ERROR = 4,              // Could not write to opened output file.

  C_BAD_ENSEMBLE_MAGIC = 5,       // Ensemble patch has bad magic.
  C_BAD_ENSEMBLE_VERSION = 6,     // Ensemble patch has wrong version.
  C_BAD_ENSEMBLE_HEADER = 7,      // Ensemble patch has corrupt header.
  C_BAD_ENSEMBLE_CRC = 8,         // Ensemble patch has corrupt data.

  C_BAD_TRANSFORM = 12,           // Transform mis-specified.
  C_BAD_BASE = 13,                // Base for transform malformed.

  C_BINARY_DIFF_CRC_ERROR = 14,   // Internal diff input doesn't have expected
                                  // CRC.

  // Internal errors.
  C_STREAM_ERROR = 20,            // Unexpected error from streams.h.
  C_STREAM_NOT_CONSUMED = 21,     // Stream has extra data, is expected to be
                                  // used up.
  C_SERIALIZATION_FAILED = 22,    //
  C_DESERIALIZATION_FAILED = 23,  //
  C_INPUT_NOT_RECOGNIZED = 24,    // Unrecognized input (not an executable).
  C_DISASSEMBLY_FAILED = 25,      //
  C_ASSEMBLY_FAILED = 26,         //
  C_ADJUSTMENT_FAILED = 27,       //
};

// What type of executable is something
// This is part of the patch format. Never reuse an id number.
enum ExecutableType {
  EXE_UNKNOWN = 0,
  EXE_WIN_32_X86 = 1,
  EXE_ELF_32_X86 = 2,
  // EXE_ELF_32_ARM_DEPRECATED = 3,  // DEPRECATED.
  EXE_WIN_32_X64 = 4,
};

class SinkStream;
class SinkStreamSet;
class SourceStream;

class AssemblyProgram;
class EncodedProgram;

// Applies the patch to the bytes in |old| and writes the transformed ensemble
// to |output|.
// Returns C_OK unless something went wrong.
Status ApplyEnsemblePatch(SourceStream* old, SourceStream* patch,
                          SinkStream* output);

// Applies the patch in |patch_file| to the bytes in |old_file| and writes the
// transformed ensemble to |new_file|.
// Returns C_OK unless something went wrong.
// This function first validates that the patch file has a proper header, so the
// function can be used to 'try' a patch.
Status ApplyEnsemblePatch(base::File old_file,
                          base::File patch_file,
                          base::File new_file);

// Applies the patch in |patch_file_name| to the bytes in |old_file_name| and
// writes the transformed ensemble to |new_file_name|.
// Returns C_OK unless something went wrong.
// This function first validates that the patch file has a proper header, so the
// function can be used to 'try' a patch.
Status ApplyEnsemblePatch(const base::FilePath::CharType* old_file_name,
                          const base::FilePath::CharType* patch_file_name,
                          const base::FilePath::CharType* new_file_name);

// Generates a patch that will transform the bytes in |old| into the bytes in
// |target|.
// Returns C_OK unless something when wrong (unexpected).
Status GenerateEnsemblePatch(SourceStream* old, SourceStream* target,
                             SinkStream* patch);

// Serializes |encoded| into the stream set.
// Returns C_OK if succeeded, otherwise returns an error status.
Status WriteEncodedProgram(EncodedProgram* encoded, SinkStreamSet* sink);

// Assembles |encoded|, emitting the bytes into |buffer|.
// Returns C_OK if succeeded, otherwise returns an error status and leaves
// |buffer| in an undefined state.
Status Assemble(EncodedProgram* encoded, SinkStream* buffer);

// Adjusts |program| to look more like |model|.
//
Status Adjust(const AssemblyProgram& model, AssemblyProgram *program);

}  // namespace courgette

#endif  // COURGETTE_COURGETTE_H_
