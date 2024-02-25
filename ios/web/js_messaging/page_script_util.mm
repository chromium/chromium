// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/page_script_util.h"

#import "base/apple/bundle_locations.h"
#import "base/strings/sys_string_conversions.h"

namespace web {

NSString* GetPageScript(NSString* script_file_name) {
  DCHECK(script_file_name);
  NSString* path =
      [base::apple::FrameworkBundle() pathForResource:script_file_name
                                               ofType:@"js"];
  DCHECK(path) << "Script file not found: "
               << base::SysNSStringToUTF8(script_file_name) << ".js";
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error) << "Error fetching script: "
                 << base::SysNSStringToUTF8(error.description);
  DCHECK(content);
  return content;
}

NSString* MakeScriptInjectableOnce(NSString* script_identifier,
                                   NSString* script) {
  NSString* kOnceWrapperTemplate =
      @"if (typeof %@ === 'undefined') { var %@ = true; %%@ }";
  NSString* injected_var_name =
      [NSString stringWithFormat:@"_injected_%@", script_identifier];
  NSString* once_wrapper =
      [NSString stringWithFormat:kOnceWrapperTemplate, injected_var_name,
                                 injected_var_name];
  return [NSString stringWithFormat:once_wrapper, script];
}

}  // namespace web
