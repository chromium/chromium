// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_COURGETTE_FLOW_H_
#define COURGETTE_COURGETTE_FLOW_H_

#include <memory>
#include <string>

#include "courgette/courgette.h"
#include "courgette/region.h"
#include "courgette/streams.h"

namespace courgette {

class AssemblyProgram;
class Disassembler;
class EncodedProgram;

// An adaptor for Region as BasicBuffer.
class RegionBuffer : public BasicBuffer {
 public:
  explicit RegionBuffer(const Region& region) : region_(region) {}

  RegionBuffer(const RegionBuffer&) = delete;
  RegionBuffer& operator=(const RegionBuffer&) = delete;

  ~RegionBuffer() override {}

  // BasicBuffer:
  const uint8_t* data() const override { return region_.start(); }
  size_t length() const override { return region_.length(); }

 private:
  Region region_;
};

// CourgetteFlow stores Courgette data arranged into groups, and exposes
// "commands" that operate on them. On the first occurrence of an error, the
// Courgette error code is recorded, error messages are generated and stored,
// and all subsequent commands become no-op. This allows callers to concisely
// specify high-level logic with minimal code for error handling.
class CourgetteFlow {
 public:
  // A group of Courgette data, for a single executable. Takes negligible space
  // when unused.
  struct Data {
    Data();
    ~Data();

    std::unique_ptr<Disassembler> disassembler;
    std::unique_ptr<AssemblyProgram> program;
    std::unique_ptr<EncodedProgram> encoded;
    SinkStreamSet sinks;
    SourceStreamSet sources;
  };

  // Group enumeration into |data_*_| fields.
  enum Group {
    ONLY,  // The only file processed.
    OLD,   // The "old" file during patching.
    NEW,   // The "new" file during patching.
  };

  CourgetteFlow();

  CourgetteFlow(const CourgetteFlow&) = delete;
  CourgetteFlow& operator=(const CourgetteFlow&) = delete;

  ~CourgetteFlow();

  static const char* name(Group group);
  Data* data(Group group);  // Allows caller to modify.
  bool ok();
  bool failed();
  Status status();
  const std::string& message();

  // Commands that perform no-op on error. This allows caller to concisely
  // specify high-level logic, and perform a single error check at the end. Care
  // must be taken w.r.t. error handling if |data()| is harvested between
  // commands.

  // Reads |buffer| to initialize |data(group)->sources|.
  void ReadSourceStreamSetFromBuffer(Group group, const BasicBuffer& buffer);

  // Reads |buffer| to initialize |data(group)->disassembler|.
  void ReadDisassemblerFromBuffer(Group group, const BasicBuffer& buffer);

  // Reads |opt_sources| if given, or else |data(group)->sources| to initialize
  // |data(group).encoded|.
  void ReadEncodedProgramFromSourceStreamSet(
      Group group,
      SourceStreamSet* opt_sources = nullptr);

  // Uses |data(group)->disassembler| to initialize |data(group)->program|,
  // passing |annotate| as initialization parameter (should be true if
  // AdjustNewAssemblyProgramToMatchOld() gets called later).
  void CreateAssemblyProgramFromDisassembler(Group group, bool annotate);

  // Uses |data(group)->disassembler| and |data(group)->program| to initialize
  // |data(group)->encoded|.
  void CreateEncodedProgramFromDisassemblerAndAssemblyProgram(Group group);

  // Serializese |data(group)->sinks| to |sink|.
  void WriteSinkStreamFromSinkStreamSet(Group group, SinkStream* sink);

  // Serializes |data(group)->encoded| to |opt_sinks| if given, or else to
  // |data(group)->sinks|.
  void WriteSinkStreamSetFromEncodedProgram(Group group,
                                            SinkStreamSet* opt_sinks = nullptr);

  // Converts |data(group)->encoded| to an exectuable and writes the result to
  // |sink|.
  void WriteExecutableFromEncodedProgram(Group group, SinkStream* sink);

  // Adjusts |data(NEW)->program| Labels to match |data(OLD)->program| Labels.
  void AdjustNewAssemblyProgramToMatchOld();

  // Destructor commands to reduce memory usage.

  void DestroyDisassembler(Group group);

  void DestroyAssemblyProgram(Group group);

  void DestroyEncodedProgram(Group group);

 private:
  // Utilities to process return values from Courgette functions, and assign
  // |status_| and |message_|. Usage:
  //   if (!check(some_courgette_function(param1, ...)))
  //     setMessage("format string %s...", value1, ...);

  // Reassigns |status_|, and returns true if |C_OK|.
  bool check(Status new_status);

  // check() alternative for functions that return true on success. On failure
  // assigns |status_| to |failure_mode|.
  bool check(bool success, Status failure_mode);

  void setMessage(const char* format, ...);

  Status status_ = C_OK;
  std::string message_;
  Data data_only_;
  Data data_old_;
  Data data_new_;
};

}  // namespace courgette

#endif  // COURGETTE_COURGETTE_FLOW_H_
