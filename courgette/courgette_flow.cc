// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/courgette_flow.h"

#include <stdarg.h>

#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "courgette/assembly_program.h"
#include "courgette/disassembler.h"
#include "courgette/encoded_program.h"
#include "courgette/program_detector.h"

namespace courgette {

/******** CourgetteFlow::Data ********/

CourgetteFlow::Data::Data() = default;

CourgetteFlow::Data::~Data() = default;

/******** CourgetteFlow ********/

CourgetteFlow::CourgetteFlow() = default;

CourgetteFlow::~CourgetteFlow() = default;

// static
const char* CourgetteFlow::name(Group group) {
  switch (group) {
    case ONLY:
      return "input";
    case OLD:
      return "'old' input";
    case NEW:
      return "'new' input";
    default:
      NOTREACHED();
      break;
  }
  return nullptr;
}

CourgetteFlow::Data* CourgetteFlow::data(Group group) {
  switch (group) {
    case ONLY:
      return &data_only_;
    case OLD:
      return &data_old_;
    case NEW:
      return &data_new_;
    default:
      NOTREACHED();
      break;
  }
  return nullptr;
}

bool CourgetteFlow::ok() {
  return status_ == C_OK;
}

bool CourgetteFlow::failed() {
  return status_ != C_OK;
}

Status CourgetteFlow::status() {
  return status_;
}

const std::string& CourgetteFlow::message() {
  return message_;
}

void CourgetteFlow::ReadSourceStreamSetFromBuffer(Group group,
                                                  const BasicBuffer& buffer) {
  if (failed())
    return;
  Data* d = data(group);
  if (!check(d->sources.Init(buffer.data(), buffer.length()),
             C_GENERAL_ERROR)) {
    setMessage("Cannot read %s as SourceStreamSet.", name(group));
  }
}

void CourgetteFlow::ReadDisassemblerFromBuffer(Group group,
                                               const BasicBuffer& buffer) {
  if (failed())
    return;
  Data* d = data(group);
  d->disassembler = DetectDisassembler(buffer.data(), buffer.length());
  if (!check(d->disassembler.get() != nullptr, C_INPUT_NOT_RECOGNIZED))
    setMessage("Cannot detect program for %s.", name(group));
}

void CourgetteFlow::ReadEncodedProgramFromSourceStreamSet(
    Group group,
    SourceStreamSet* opt_sources /* nullptr */) {
  if (failed())
    return;
  Data* d = data(group);
  SourceStreamSet* sources = opt_sources ? opt_sources : &d->sources;
  if (!check(ReadEncodedProgram(sources, &d->encoded)))
    setMessage("Cannot read %s as encoded program.", name(group));
}

void CourgetteFlow::CreateAssemblyProgramFromDisassembler(Group group,
                                                          bool annotate) {
  if (failed())
    return;
  Data* d = data(group);
  d->program = d->disassembler->CreateProgram(annotate);
  if (!check(d->program.get() != nullptr, C_DISASSEMBLY_FAILED))
    setMessage("Cannot create AssemblyProgram for %s.", name(group));
}

void CourgetteFlow::CreateEncodedProgramFromDisassemblerAndAssemblyProgram(
    Group group) {
  if (failed())
    return;
  Data* d = data(group);
  d->encoded = std::make_unique<EncodedProgram>();
  if (!check(d->disassembler->DisassembleAndEncode(d->program.get(),
                                                   d->encoded.get()))) {
    setMessage("Cannot disassemble to form EncodedProgram for %s.",
               name(group));
  }
}

void CourgetteFlow::WriteSinkStreamFromSinkStreamSet(Group group,
                                                     SinkStream* sink) {
  DCHECK(sink);
  if (failed())
    return;
  if (!check(data(group)->sinks.CopyTo(sink), C_GENERAL_ERROR))
    setMessage("Cannnot combine serialized streams for %s.", name(group));
}

void CourgetteFlow::WriteSinkStreamSetFromEncodedProgram(
    Group group,
    SinkStreamSet* opt_sinks /* nullptr */) {
  if (failed())
    return;
  Data* d = data(group);
  SinkStreamSet* sinks = opt_sinks ? opt_sinks : &d->sinks;
  if (!check(WriteEncodedProgram(d->encoded.get(), sinks)))
    setMessage("Cannot serialize encoded %s.", name(group));
}

void CourgetteFlow::WriteExecutableFromEncodedProgram(Group group,
                                                      SinkStream* sink) {
  DCHECK(sink);
  if (failed())
    return;
  if (!check(Assemble(data(group)->encoded.get(), sink)))
    setMessage("Cannot assemble %s.", name(group));
}

void CourgetteFlow::AdjustNewAssemblyProgramToMatchOld() {
  if (failed())
    return;
  if (!check(Adjust(*data_old_.program, data_new_.program.get())))
    setMessage("Cannot adjust %s to match %s.", name(OLD), name(NEW));
}

void CourgetteFlow::DestroyDisassembler(Group group) {
  if (failed())
    return;
  data(group)->disassembler.reset();
}

void CourgetteFlow::DestroyAssemblyProgram(Group group) {
  if (failed())
    return;
  data(group)->program.reset();
}

void CourgetteFlow::DestroyEncodedProgram(Group group) {
  if (failed())
    return;
  data(group)->encoded.reset();
}

bool CourgetteFlow::check(Status new_status) {
  if (new_status == C_OK)
    return true;
  status_ = new_status;
  return false;
}

bool CourgetteFlow::check(bool success, Status failure_mode) {
  if (success)
    return true;
  status_ = failure_mode;
  return false;
}

void CourgetteFlow::setMessage(const char* format, ...) {
  va_list args;
  va_start(args, format);
  message_ = base::StringPrintV(format, args);
  va_end(args);
}

}  // namespace courgette
