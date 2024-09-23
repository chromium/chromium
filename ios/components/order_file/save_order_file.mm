// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/order_file/save_order_file.h"

#import <Foundation/Foundation.h>
#import <dlfcn.h>
#import <libkern/OSAtomicQueue.h>

#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/components/order_file/order_file_common.h"

namespace {

// Returns the directory used to save generated order files.
NSString* CRWGetOutputsDirectory() {
  NSArray<NSString*>* paths = NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES);
  NSString* path =
      [paths.firstObject stringByAppendingPathComponent:@"order_file"];
  path = [path stringByStandardizingPath];

  NSFileManager* fileManager = [NSFileManager defaultManager];
  BOOL isDir;
  if ([fileManager fileExistsAtPath:path isDirectory:&isDir]) {
    if (!isDir) {
      @throw [NSException
          exceptionWithName:@"OrderFileGenerationError"
                     reason:[NSString
                                stringWithFormat:
                                    @"Could not create directory %@ for output",
                                    path]
                   userInfo:nil];
    }
  } else {
    NSError* error = nil;
    BOOL success = [fileManager createDirectoryAtPath:path
                          withIntermediateDirectories:YES
                                           attributes:nil
                                                error:&error];
    if (success) {
      LOG(WARNING) << "Created dir: " << base::SysNSStringToUTF8(path) << "\n";
    } else {
      @throw [NSException
          exceptionWithName:@"OrderFileGenerationError"
                     reason:[NSString
                                stringWithFormat:
                                    @"Failed to create directory %@ for output",
                                    path]
                   userInfo:nil];
    }
  }
  return path;
}

// Dedups function calls and write order file to disk.
BOOL CRWDedupAndSaveOrderFile(NSString* fileName,
                              NSArray<NSString*>* functions) {
  // Dedup and reverse function calls. This is done in a separate loop because
  // it is important that the first call of each function is the one that is
  // kept.
  NSMutableArray<NSString*>* uniqueFunctionCalls =
      [NSMutableArray arrayWithCapacity:functions.count];
  NSMutableSet<NSString*>* storedFunctionCalls = [[NSMutableSet alloc] init];
  NSEnumerator<NSString*>* enumerator = [functions reverseObjectEnumerator];
  NSString* functionName;
  while ((functionName = [enumerator nextObject])) {
    if (uniqueFunctionCalls.count % 1000 == 0) {
      // Print once every 1000 times to save some time while de-queuing.
      LOG(WARNING) << "Reordering and deduping functions: "
                    << uniqueFunctionCalls.count << " unique\n";
    }

    if (![storedFunctionCalls containsObject:functionName]) {
      [uniqueFunctionCalls addObject:functionName];
      [storedFunctionCalls addObject:functionName];
    }
  }

  LOG(WARNING) << "Saving order file for " << base::SysNSStringToUTF8(fileName)
                << ". " << uniqueFunctionCalls.count
                << " unique function calls recorded.\n";

  [uniqueFunctionCalls addObject:@""];  // Adding a new line to end of the file.
  NSString* output = [uniqueFunctionCalls componentsJoinedByString:@"\n"];

  // Save order file to disk.
  NSString* filePath = [CRWGetOutputsDirectory()
      stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.order",
                                                                fileName]];
  NSData* fileContents = [output dataUsingEncoding:NSUTF8StringEncoding];
  NSError* error;
  BOOL success = [fileContents writeToFile:filePath
                                   options:NSDataWritingFileProtectionNone
                                     error:&error];
  if (success) {
    LOG(WARNING) << "Order file saved to path: "
                  << base::SysNSStringToUTF8(filePath) << "\n";
  } else {
    LOG(WARNING) << "Order file save failed. Path: "
                  << base::SysNSStringToUTF8(filePath) << "\n, Error: "
                  << base::SysNSStringToUTF8(error.debugDescription) << "\n";
  }
  return success;
}

}  // namespace

extern "C" {

// Finish sanitizer collecting and dump order files.
void CRWSaveOrderFile() {
  // Disable collection of further procedure calls.
  if (gCRWFinishedCollecting) {
    return;
  }
  gCRWFinishedCollecting = YES;
  __sync_synchronize();
  LOG(WARNING) << "Generating order file.\n";

  // If this function is called, the app is being run to generate an order file,
  // so a blocking thread can be used.
  NSMutableArray<NSString*>* allFunctions = [NSMutableArray array];
  NSUInteger procedureCallCount = 0;
  while (YES) {
    if (procedureCallCount % 1000 == 0) {
      // Print once every 1000 times to save some time while de-queuing.
      LOG(WARNING) << "Dequeuing functions: " << allFunctions.count
                    << " valid / " << procedureCallCount << " total\n";
    }
    CRWProcedureCallNode* node = (CRWProcedureCallNode*)OSAtomicDequeue(
        &gCRWSanitizerQueue, offsetof(CRWProcedureCallNode, next));
    if (node == nullptr) {
      break;
    }
    Dl_info dlInfo;
    if (dladdr(node->procedureCall, &dlInfo) == 0 || !dlInfo.dli_sname) {
      continue;
    }
    procedureCallCount++;

    NSString* functionName = @(dlInfo.dli_sname);
    BOOL isObjc =
        [functionName hasPrefix:@"+["] || [functionName hasPrefix:@"-["];
    functionName =
        isObjc ? functionName : [@"_" stringByAppendingString:functionName];

    [allFunctions addObject:functionName];
  }

  if (allFunctions.count > 0) {
    LOG(WARNING) << "Out of " << procedureCallCount
                  << " recorded function calls, " << allFunctions.count
                  << " had a valid function name.\n";
  } else {
    LOG(WARNING) << "No functions found in order file generation.\n";
    return;
  }

  BOOL success = CRWDedupAndSaveOrderFile(@"app", allFunctions);
  if (success) {
    LOG(WARNING) << "ORDER_FILE_DUMPED\n";
    exit(0);
  } else {
    LOG(WARNING) << "ORDER_FILE_DUMP_FAILED\n";
    exit(1);
  }
}

}  // extern "C"
