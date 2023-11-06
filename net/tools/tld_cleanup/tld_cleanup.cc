// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This command-line program converts an effective-TLD data file in UTF-8 from
// the format provided by Mozilla to the format expected by Chrome.  This
// program generates an intermediate file which is then used by gperf to
// generate a perfect hash map.  The benefit of this approach is that no time is
// spent on program initialization to generate the map of this data.
//
// Running this program finds "effective_tld_names.dat" in the expected location
// in the source checkout and generates "effective_tld_names.gperf" next to it.
//
// Any errors or warnings from this program are recorded in tld_cleanup.log.
//
// In particular, it
//  * Strips blank lines and comments, as well as notes for individual rules.
//  * Strips a single leading and/or trailing dot from each rule, if present.
//  * Logs a warning if a rule contains '!' or '*.' other than at the beginning
//    of the rule.  (This also catches multiple ! or *. at the start of a rule.)
//  * Logs a warning if GURL reports a rule as invalid, but keeps the rule.
//  * Canonicalizes each rule's domain by converting it to a GURL and back.
//  * Adds explicit rules for true TLDs found in any rule.
//  * Marks entries in the file between "// ===BEGIN PRIVATE DOMAINS==="
//    and "// ===END PRIVATE DOMAINS===" as private.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/memory.h"
#include "net/tools/tld_cleanup/tld_cleanup_util.h"

int main(int argc, const char* argv[]) {
  base::EnableTerminationOnHeapCorruption();
  if (argc != 1) {
    fprintf(stderr, "Normalizes and verifies UTF-8 TLD data files\n");
    fprintf(stderr, "Usage: %s\n", argv[0]);
    return 1;
  }

  // Manages the destruction of singletons.
  base::AtExitManager exit_manager;

  // Only use OutputDebugString in debug mode.
#ifdef NDEBUG
  logging::LoggingDestination destination = logging::LOG_TO_FILE;
#else
  logging::LoggingDestination destination =
      logging::LOG_TO_ALL;
#endif

  base::CommandLine::Init(argc, argv);

  base::FilePath log_filename;
  base::PathService::Get(base::DIR_EXE, &log_filename);
  log_filename = log_filename.AppendASCII("tld_cleanup.log");
  logging::LoggingSettings settings;
  settings.logging_dest = destination;
  settings.log_file_path = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);

  base::i18n::InitializeICU();

  base::FilePath input_file;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &input_file);
  input_file = input_file.Append(FILE_PATH_LITERAL("net"))
                         .Append(FILE_PATH_LITERAL("base"))
                         .Append(FILE_PATH_LITERAL(
                             "registry_controlled_domains"))
                         .Append(FILE_PATH_LITERAL("effective_tld_names.dat"));
  base::FilePath output_file;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &output_file);
  output_file = output_file.Append(FILE_PATH_LITERAL("net"))
                           .Append(FILE_PATH_LITERAL("base"))
                           .Append(FILE_PATH_LITERAL(
                               "registry_controlled_domains"))
                           .Append(FILE_PATH_LITERAL(
                               "effective_tld_names.gperf"));
  net::tld_cleanup::NormalizeResult result =
      net::tld_cleanup::NormalizeFile(input_file, output_file);
  if (result != net::tld_cleanup::NormalizeResult::kSuccess) {
    fprintf(stderr,
            "Errors or warnings processing file.  See log in tld_cleanup.log.");
  }

  if (result == net::tld_cleanup::NormalizeResult::kError)
    return 1;
  return 0;
}
