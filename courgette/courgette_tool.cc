// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/courgette_flow.h"
#include "courgette/encoded_program.h"
#include "courgette/program_detector.h"
#include "courgette/streams.h"
#include "courgette/third_party/bsdiff/bsdiff.h"

namespace {

using courgette::CourgetteFlow;

const char kUsageGen[] = "-gen <old_in> <new_in> <patch_out>";
const char kUsageApply[] = "-apply <old_in> <patch_in> <new_out>";
const char kUsageGenbsdiff[] = "-genbsdiff <old_in> <new_in> <patch_out>";
const char kUsageApplybsdiff[] = "-applybsdiff <old_in> <patch_in> <new_out>";
const char kUsageSupported[] = "-supported <exec_file_in>";
const char kUsageDis[] = "-dis <exec_file_in> <assembly_file_out>";
const char kUsageAsm[] = "-asm <assembly_file_in> <exec_file_out>";
const char kUsageDisadj[] = "-disadj <old_in> <new_in> <new_assembly_file_out>";
const char kUsageGen1[] = "-gen1[au] <old_in> <new_in> <patch_base_out>";

/******** Utilities to print help and exit ********/

void PrintHelp() {
  fprintf(stderr, "Main Usage:\n");
  for (auto usage :
       {kUsageGen, kUsageApply, kUsageGenbsdiff, kUsageApplybsdiff}) {
    fprintf(stderr, "  courgette %s\n", usage);
  }
  fprintf(stderr, "Diagnosis Usage:\n");
  for (auto usage :
       {kUsageSupported, kUsageDis, kUsageAsm, kUsageDisadj, kUsageGen1}) {
    fprintf(stderr, "  courgette %s\n", usage);
  }
}

void UsageProblem(const char* message) {
  fprintf(stderr, "%s", message);
  fprintf(stderr, "\n");
  PrintHelp();
  exit(1);
}

void Problem(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}

/******** BufferedFileReader ********/

// A file reader that calls Problem() on failure.
class BufferedFileReader : public courgette::BasicBuffer {
 public:
  BufferedFileReader(const base::FilePath& file_name, const char* kind) {
    if (!buffer_.Initialize(file_name))
      Problem("Can't read %s file.", kind);
  }
  ~BufferedFileReader() override = default;

  // courgette::BasicBuffer:
  const uint8_t* data() const override { return buffer_.data(); }
  size_t length() const override { return buffer_.length(); }

 private:
  base::MemoryMappedFile buffer_;

  DISALLOW_COPY_AND_ASSIGN(BufferedFileReader);
};

/******** Various helpers ********/

void WriteSinkToFile(const courgette::SinkStream* sink,
                     const base::FilePath& output_file) {
  int count = base::WriteFile(output_file,
                              reinterpret_cast<const char*>(sink->Buffer()),
                              static_cast<int>(sink->Length()));
  if (count == -1)
    Problem("Can't write output.");
  if (static_cast<size_t>(count) != sink->Length())
    Problem("Incomplete write.");
}

bool Supported(const base::FilePath& input_file) {
  bool result = false;

  BufferedFileReader buffer(input_file, "input");

  courgette::ExecutableType type;
  size_t detected_length;

  DetectExecutableType(buffer.data(), buffer.length(), &type, &detected_length);

  // If the detection fails, we just fall back on UNKNOWN
  std::string format = "Unsupported";

  switch (type) {
    case courgette::EXE_UNKNOWN:
      break;

    case courgette::EXE_WIN_32_X86:
      format = "Windows 32 PE";
      result = true;
      break;

    case courgette::EXE_ELF_32_X86:
      format = "ELF 32 X86";
      result = true;
      break;

    case courgette::EXE_ELF_32_ARM:
      format = "ELF 32 ARM";
      result = true;
      break;

    case courgette::EXE_WIN_32_X64:
      format = "Windows 64 PE";
      result = true;
      break;
  }

  printf("%s Executable\n", format.c_str());
  return result;
}

void Disassemble(const base::FilePath& input_file,
                 const base::FilePath& output_file) {
  CourgetteFlow flow;
  BufferedFileReader input_buffer(input_file, flow.name(flow.ONLY));
  flow.ReadDisassemblerFromBuffer(flow.ONLY, input_buffer);
  flow.CreateAssemblyProgramFromDisassembler(flow.ONLY, false);
  flow.CreateEncodedProgramFromDisassemblerAndAssemblyProgram(flow.ONLY);
  flow.DestroyDisassembler(flow.ONLY);
  flow.DestroyAssemblyProgram(flow.ONLY);
  flow.WriteSinkStreamSetFromEncodedProgram(flow.ONLY);
  flow.DestroyEncodedProgram(flow.ONLY);
  courgette::SinkStream sink;
  flow.WriteSinkStreamFromSinkStreamSet(flow.ONLY, &sink);
  if (flow.failed())
    Problem(flow.message().c_str());

  WriteSinkToFile(&sink, output_file);
}

void DisassembleAndAdjust(const base::FilePath& old_file,
                          const base::FilePath& new_file,
                          const base::FilePath& output_file) {
  // Flow graph and process sequence (DA = Disassembler, AP = AssemblyProgram,
  // EP = EncodedProgram, Adj = Adjusted):
  //   [1 Old DA] --> [2 Old AP]    [4 New AP] <-- [3 New DA]
  //                      |             |              |
  //                      |             v (move)       v
  //                      +---> [5 Adj New AP] --> [6 New EP]
  //                                               (7 Write)
  CourgetteFlow flow;
  BufferedFileReader old_buffer(old_file, flow.name(flow.OLD));
  BufferedFileReader new_buffer(new_file, flow.name(flow.NEW));
  flow.ReadDisassemblerFromBuffer(flow.OLD, old_buffer);       // 1
  flow.CreateAssemblyProgramFromDisassembler(flow.OLD, true);  // 2
  flow.DestroyDisassembler(flow.OLD);
  flow.ReadDisassemblerFromBuffer(flow.NEW, new_buffer);       // 3
  flow.CreateAssemblyProgramFromDisassembler(flow.NEW, true);  // 4
  flow.AdjustNewAssemblyProgramToMatchOld();                   // 5
  flow.DestroyAssemblyProgram(flow.OLD);
  flow.CreateEncodedProgramFromDisassemblerAndAssemblyProgram(flow.NEW);  // 6
  flow.DestroyAssemblyProgram(flow.NEW);
  flow.DestroyDisassembler(flow.NEW);
  flow.WriteSinkStreamSetFromEncodedProgram(flow.NEW);  // 7
  flow.DestroyEncodedProgram(flow.NEW);
  courgette::SinkStream sink;
  flow.WriteSinkStreamFromSinkStreamSet(flow.NEW, &sink);
  if (flow.failed())
    Problem(flow.message().c_str());

  WriteSinkToFile(&sink, output_file);
}

// Diffs two executable files, write a set of files for the diff, one file per
// stream of the EncodedProgram format.  Each file is the bsdiff between the
// original file's stream and the new file's stream.  This is completely
// uninteresting to users, but it is handy for seeing how much each which
// streams are contributing to the final file size.  Adjustment is optional.
void DisassembleAdjustDiff(const base::FilePath& old_file,
                           const base::FilePath& new_file,
                           const base::FilePath& output_file_root,
                           bool adjust) {
  // Same as PatchGeneratorX86_32::Transform(), except Adjust is optional, and
  // |flow|'s internal SinkStreamSet get used.
  // Flow graph and process sequence (DA = Disassembler, AP = AssemblyProgram,
  // EP = EncodedProgram, Adj = Adjusted):
  //   [1 Old DA] --> [2 Old AP]   [6 New AP] <-- [5 New DA]
  //       |            |   |          |              |
  //       v            |   |          v (move)       v
  //   [3 Old EP] <-----+   +->[7 Adj New AP] --> [8 New EP]
  //   (4 Write)                                  (9 Write)
  CourgetteFlow flow;
  BufferedFileReader old_buffer(old_file, flow.name(flow.OLD));
  BufferedFileReader new_buffer(new_file, flow.name(flow.NEW));
  flow.ReadDisassemblerFromBuffer(flow.OLD, old_buffer);                  // 1
  flow.CreateAssemblyProgramFromDisassembler(flow.OLD, adjust);           // 2
  flow.CreateEncodedProgramFromDisassemblerAndAssemblyProgram(flow.OLD);  // 3
  flow.DestroyDisassembler(flow.OLD);
  flow.WriteSinkStreamSetFromEncodedProgram(flow.OLD);  // 4
  flow.DestroyEncodedProgram(flow.OLD);
  flow.ReadDisassemblerFromBuffer(flow.NEW, new_buffer);         // 5
  flow.CreateAssemblyProgramFromDisassembler(flow.NEW, adjust);  // 6
  if (adjust)
    flow.AdjustNewAssemblyProgramToMatchOld();  // 7, optional
  flow.DestroyAssemblyProgram(flow.OLD);
  flow.CreateEncodedProgramFromDisassemblerAndAssemblyProgram(flow.NEW);  // 8
  flow.DestroyAssemblyProgram(flow.NEW);
  flow.DestroyDisassembler(flow.NEW);
  flow.WriteSinkStreamSetFromEncodedProgram(flow.NEW);  // 9
  flow.DestroyEncodedProgram(flow.NEW);
  if (flow.failed())
    Problem(flow.message().c_str());

  courgette::SinkStream empty_sink;
  for (int i = 0;; ++i) {
    courgette::SinkStream* old_stream = flow.data(flow.OLD)->sinks.stream(i);
    courgette::SinkStream* new_stream = flow.data(flow.NEW)->sinks.stream(i);
    if (old_stream == nullptr && new_stream == nullptr)
      break;

    courgette::SourceStream old_source;
    courgette::SourceStream new_source;
    old_source.Init(old_stream ? *old_stream : empty_sink);
    new_source.Init(new_stream ? *new_stream : empty_sink);
    courgette::SinkStream patch_stream;
    bsdiff::BSDiffStatus status =
        bsdiff::CreateBinaryPatch(&old_source, &new_source, &patch_stream);
    if (status != bsdiff::OK)
      Problem("-xxx failed.");

    std::string append = std::string("-") + base::NumberToString(i);

    WriteSinkToFile(&patch_stream,
                    output_file_root.InsertBeforeExtensionASCII(append));
  }
}

void Assemble(const base::FilePath& input_file,
              const base::FilePath& output_file) {
  CourgetteFlow flow;
  BufferedFileReader input_buffer(input_file, flow.name(flow.ONLY));
  flow.ReadSourceStreamSetFromBuffer(flow.ONLY, input_buffer);
  flow.ReadEncodedProgramFromSourceStreamSet(flow.ONLY);
  courgette::SinkStream sink;
  flow.WriteExecutableFromEncodedProgram(flow.ONLY, &sink);
  if (flow.failed())
    Problem(flow.message().c_str());

  WriteSinkToFile(&sink, output_file);
}

void GenerateEnsemblePatch(const base::FilePath& old_file,
                           const base::FilePath& new_file,
                           const base::FilePath& patch_file) {
  BufferedFileReader old_buffer(old_file, "'old' input");
  BufferedFileReader new_buffer(new_file, "'new' input");

  courgette::SourceStream old_stream;
  courgette::SourceStream new_stream;
  old_stream.Init(old_buffer.data(), old_buffer.length());
  new_stream.Init(new_buffer.data(), new_buffer.length());

  courgette::SinkStream patch_stream;
  courgette::Status status =
      courgette::GenerateEnsemblePatch(&old_stream, &new_stream, &patch_stream);

  if (status != courgette::C_OK)
    Problem("-gen failed.");

  WriteSinkToFile(&patch_stream, patch_file);
}

void ApplyEnsemblePatch(const base::FilePath& old_file,
                        const base::FilePath& patch_file,
                        const base::FilePath& new_file) {
  // We do things a little differently here in order to call the same Courgette
  // entry point as the installer.  That entry point point takes file names and
  // returns an status code but does not output any diagnostics.

  courgette::Status status = courgette::ApplyEnsemblePatch(
      old_file.value().c_str(), patch_file.value().c_str(),
      new_file.value().c_str());

  if (status == courgette::C_OK)
    return;

  // Diagnose the error.
  switch (status) {
    case courgette::C_BAD_ENSEMBLE_MAGIC:
      Problem("Not a courgette patch");
      break;

    case courgette::C_BAD_ENSEMBLE_VERSION:
      Problem("Wrong version patch");
      break;

    case courgette::C_BAD_ENSEMBLE_HEADER:
      Problem("Corrupt patch");
      break;

    case courgette::C_DISASSEMBLY_FAILED:
      Problem("Disassembly failed (could be because of memory issues)");
      break;

    case courgette::C_STREAM_ERROR:
      Problem("Stream error (likely out of memory or disk space)");
      break;

    default:
      break;
  }

  // If we failed due to a missing input file, this will print the message.
  { BufferedFileReader old_buffer(old_file, "'old' input"); }
  { BufferedFileReader patch_buffer(patch_file, "'patch' input"); }

  // Non-input related errors:
  if (status == courgette::C_WRITE_OPEN_ERROR)
    Problem("Can't open output");
  if (status == courgette::C_WRITE_ERROR)
    Problem("Can't write output");

  Problem("-apply failed.");
}

void GenerateBSDiffPatch(const base::FilePath& old_file,
                         const base::FilePath& new_file,
                         const base::FilePath& patch_file) {
  BufferedFileReader old_buffer(old_file, "'old' input");
  BufferedFileReader new_buffer(new_file, "'new' input");

  courgette::SourceStream old_stream;
  courgette::SourceStream new_stream;
  old_stream.Init(old_buffer.data(), old_buffer.length());
  new_stream.Init(new_buffer.data(), new_buffer.length());

  courgette::SinkStream patch_stream;
  bsdiff::BSDiffStatus status =
      bsdiff::CreateBinaryPatch(&old_stream, &new_stream, &patch_stream);

  if (status != bsdiff::OK)
    Problem("-genbsdiff failed.");

  WriteSinkToFile(&patch_stream, patch_file);
}

void ApplyBSDiffPatch(const base::FilePath& old_file,
                      const base::FilePath& patch_file,
                      const base::FilePath& new_file) {
  BufferedFileReader old_buffer(old_file, "'old' input");
  BufferedFileReader patch_buffer(patch_file, "'patch' input");

  courgette::SourceStream old_stream;
  courgette::SourceStream patch_stream;
  old_stream.Init(old_buffer.data(), old_buffer.length());
  patch_stream.Init(patch_buffer.data(), patch_buffer.length());

  courgette::SinkStream new_stream;
  bsdiff::BSDiffStatus status =
      bsdiff::ApplyBinaryPatch(&old_stream, &patch_stream, &new_stream);

  if (status != bsdiff::OK)
    Problem("-applybsdiff failed.");

  WriteSinkToFile(&new_stream, new_file);
}

}  // namespace

int main(int argc, const char* argv[]) {
  base::AtExitManager at_exit_manager;
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  logging::LoggingSettings settings;
  if (command_line.HasSwitch("nologfile")) {
    settings.logging_dest =
        logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  } else {
    settings.logging_dest = logging::LOG_TO_ALL;
    settings.log_file_path = FILE_PATH_LITERAL("courgette.log");
  }
  (void)logging::InitLogging(settings);
  logging::SetMinLogLevel(logging::LOG_VERBOSE);

  bool cmd_sup = command_line.HasSwitch("supported");
  bool cmd_dis = command_line.HasSwitch("dis");
  bool cmd_asm = command_line.HasSwitch("asm");
  bool cmd_disadj = command_line.HasSwitch("disadj");
  bool cmd_make_patch = command_line.HasSwitch("gen");
  bool cmd_apply_patch = command_line.HasSwitch("apply");
  bool cmd_make_bsdiff_patch = command_line.HasSwitch("genbsdiff");
  bool cmd_apply_bsdiff_patch = command_line.HasSwitch("applybsdiff");
  bool cmd_spread_1_adjusted = command_line.HasSwitch("gen1a");
  bool cmd_spread_1_unadjusted = command_line.HasSwitch("gen1u");

  std::vector<base::FilePath> values;
  const base::CommandLine::StringVector& args = command_line.GetArgs();
  for (size_t i = 0; i < args.size(); ++i) {
    values.push_back(base::FilePath(args[i]));
  }

  // '-repeat=N' is for debugging.  Running many iterations can reveal leaks and
  // bugs in cleanup.
  int repeat_count = 1;
  std::string repeat_switch = command_line.GetSwitchValueASCII("repeat");
  if (!repeat_switch.empty())
    if (!base::StringToInt(repeat_switch, &repeat_count))
      repeat_count = 1;

  if (cmd_sup + cmd_dis + cmd_asm + cmd_disadj + cmd_make_patch +
          cmd_apply_patch + cmd_make_bsdiff_patch + cmd_apply_bsdiff_patch +
          cmd_spread_1_adjusted + cmd_spread_1_unadjusted !=
      1) {
    UsageProblem(
        "First argument must be one of:\n"
        "  -supported, -asm, -dis, -disadj, -gen, -apply, -genbsdiff,"
        " -applybsdiff, or -gen1[au].");
  }

  while (repeat_count-- > 0) {
    if (cmd_sup) {
      if (values.size() != 1)
        UsageProblem(kUsageSupported);
      return !Supported(values[0]);
    } else if (cmd_dis) {
      if (values.size() != 2)
        UsageProblem(kUsageDis);
      Disassemble(values[0], values[1]);
    } else if (cmd_asm) {
      if (values.size() != 2)
        UsageProblem(kUsageAsm);
      Assemble(values[0], values[1]);
    } else if (cmd_disadj) {
      if (values.size() != 3)
        UsageProblem(kUsageDisadj);
      DisassembleAndAdjust(values[0], values[1], values[2]);
    } else if (cmd_make_patch) {
      if (values.size() != 3)
        UsageProblem(kUsageGen);
      GenerateEnsemblePatch(values[0], values[1], values[2]);
    } else if (cmd_apply_patch) {
      if (values.size() != 3)
        UsageProblem(kUsageApply);
      ApplyEnsemblePatch(values[0], values[1], values[2]);
    } else if (cmd_make_bsdiff_patch) {
      if (values.size() != 3)
        UsageProblem(kUsageGenbsdiff);
      GenerateBSDiffPatch(values[0], values[1], values[2]);
    } else if (cmd_apply_bsdiff_patch) {
      if (values.size() != 3)
        UsageProblem(kUsageApplybsdiff);
      ApplyBSDiffPatch(values[0], values[1], values[2]);
    } else if (cmd_spread_1_adjusted || cmd_spread_1_unadjusted) {
      if (values.size() != 3)
        UsageProblem(kUsageGen1);
      DisassembleAdjustDiff(values[0], values[1], values[2],
                            cmd_spread_1_adjusted);
    } else {
      UsageProblem("No operation specified");
    }
  }

  return 0;
}
