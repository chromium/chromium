// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_validator.h"

#import <string>
#import <utility>

#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"
#import "url/gurl.h"

namespace {

NSString* ToNSString(std::string_view str) {
  return base::SysUTF8ToNSString(str);
}

// Converts a boolean-like Obj-C object into an optional bool.
std::optional<bool> ParseBooleanValue(id value) {
  if (!value) {
    return std::nullopt;
  }
  if ([value isKindOfClass:[NSNumber class]]) {
    return [(NSNumber*)value boolValue];
  }
  if ([value isKindOfClass:[NSString class]]) {
    NSString* str = [(NSString*)value lowercaseString];
    if ([str isEqualToString:@"true"] || [str isEqualToString:@"yes"] ||
        [str isEqualToString:@"1"]) {
      return true;
    }
    if ([str isEqualToString:@"false"] || [str isEqualToString:@"no"] ||
        [str isEqualToString:@"0"]) {
      return false;
    }
  }
  return std::nullopt;
}

// Serializes a protobuf action to NSData.
NSData* SerializeActionProto(const optimization_guide::proto::Action& action) {
  std::string serialized;
  action.SerializeToString(&serialized);
  return [NSData dataWithBytes:serialized.data() length:serialized.size()];
}

}  // namespace

@implementation TTCToolValidator

#pragma mark - Public

+ (base::expected<void, NSString*>)validateToolName:(NSString*)name
                                          arguments:(NSDictionary*)arguments
                                             callID:(NSString*)callID {
  if (!name || ![name isKindOfClass:[NSString class]] || [name length] == 0 ||
      !callID || ![callID isKindOfClass:[NSString class]] ||
      [callID length] == 0) {
    return base::unexpected(ToNSString(ttc::kErrorMessageInvalidToolCall));
  }
  if (arguments && ![arguments isKindOfClass:[NSDictionary class]]) {
    return base::unexpected(ToNSString(ttc::kErrorMessageInvalidArguments));
  }

  if ([name isEqualToString:ToNSString(ttc::kToolGoBack)] ||
      [name isEqualToString:ToNSString(ttc::kToolGoForward)]) {
    return base::ok();
  }

  if ([name isEqualToString:ToNSString(ttc::kToolOpenUrl)]) {
    id urlValue = arguments ? arguments[@"url"] : nil;
    if (!urlValue || ![urlValue isKindOfClass:[NSString class]] ||
        [urlValue length] == 0) {
      return base::unexpected(ToNSString(ttc::kErrorMessageInvalidArguments));
    }

    std::string urlString = base::SysNSStringToUTF8(urlValue);
    GURL gurl(urlString);
    if (!gurl.is_valid() || !gurl.SchemeIsHTTPOrHTTPS()) {
      return base::unexpected(ToNSString(ttc::kErrorMessageInvalidUrl));
    }

    id newTabValue = arguments ? arguments[@"new_tab"] : nil;
    if (newTabValue) {
      auto parsedBool = ParseBooleanValue(newTabValue);
      if (!parsedBool.has_value() || *parsedBool) {
        return base::unexpected(
            ToNSString(ttc::kErrorMessageNewTabUnsupported));
      }
    }

    return base::ok();
  }

  return base::unexpected(ToNSString(ttc::kErrorMessageUnknownTool));
}

+ (base::expected<TTCActuationRequest*, NSString*>)
    createActuationRequestWithToolName:(NSString*)name
                             arguments:(NSDictionary*)arguments
                                callID:(NSString*)callID {
  auto validation = [self validateToolName:name
                                 arguments:arguments
                                    callID:callID];
  if (!validation.has_value()) {
    return base::unexpected(validation.error());
  }

  optimization_guide::proto::Action action;
  if ([name isEqualToString:ToNSString(ttc::kToolOpenUrl)]) {
    NSString* urlString = arguments[@"url"];
    GURL gurl(base::SysNSStringToUTF8(urlString));
    action.mutable_navigate()->set_url(gurl.spec());
  } else if ([name isEqualToString:ToNSString(ttc::kToolGoBack)]) {
    action.mutable_back();
  } else if ([name isEqualToString:ToNSString(ttc::kToolGoForward)]) {
    action.mutable_forward();
  }

  NSData* actionData = SerializeActionProto(action);
  TTCActuationRequest* request =
      [[TTCActuationRequest alloc] initWithActionProtos:@[ actionData ]
                                             taskUpdate:@"Executing TTC tool"
                                                 callID:callID];
  return request;
}

+ (base::expected<TTCActuationRequest*, NSString*>)
    createActuationRequestWithToolCalls:(NSArray<NSDictionary*>*)toolCalls {
  if (!toolCalls || ![toolCalls isKindOfClass:[NSArray class]] ||
      toolCalls.count == 0) {
    return base::unexpected(ToNSString(ttc::kErrorMessageEmptyToolCalls));
  }

  NSMutableArray<NSData*>* actionProtos = [NSMutableArray array];
  NSMutableArray<NSString*>* callIDs = [NSMutableArray array];

  for (id item in toolCalls) {
    if (![item isKindOfClass:[NSDictionary class]]) {
      return base::unexpected(ToNSString(ttc::kErrorMessageInvalidToolCall));
    }

    NSDictionary* toolCall = (NSDictionary*)item;
    NSString* name = toolCall[@"name"];
    NSString* callID = toolCall[@"id"];
    NSDictionary* args = toolCall[@"args"] ?: @{};

    auto validation = [self validateToolName:name arguments:args callID:callID];
    if (!validation.has_value()) {
      return base::unexpected(validation.error());
    }

    [callIDs addObject:callID];

    optimization_guide::proto::Action action;
    if ([name isEqualToString:ToNSString(ttc::kToolOpenUrl)]) {
      NSString* urlString = args[@"url"];
      GURL gurl(base::SysNSStringToUTF8(urlString));
      action.mutable_navigate()->set_url(gurl.spec());
    } else if ([name isEqualToString:ToNSString(ttc::kToolGoBack)]) {
      action.mutable_back();
    } else if ([name isEqualToString:ToNSString(ttc::kToolGoForward)]) {
      action.mutable_forward();
    }

    [actionProtos addObject:SerializeActionProto(action)];
  }

  TTCActuationRequest* request =
      [[TTCActuationRequest alloc] initWithActionProtos:actionProtos
                                             taskUpdate:@"Executing TTC tools"
                                                callIDs:callIDs];
  return request;
}

@end
