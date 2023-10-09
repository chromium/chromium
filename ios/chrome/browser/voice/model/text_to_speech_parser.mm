// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/model/text_to_speech_parser.h"

#import "base/base64.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace {

// The start and end tags that delimit TTS audio data.
NSString* const kTTSStartTag = @"function(){var _a_tts='";
NSString* const kTTSEndTag = @"'";

// When `kTTSAudioDataExtractorScriptFormat` is evaluated on a Google voice
// search results page, this script will extract the innerHTML from the script
// element containing TTS data.  The format takes one parameter, which is the
// start tag from the TTS config singleton.
NSString* const kTTSAudioDataExtractorScriptFormat =
    @"(function(){"
     "  var start_tag = \"%@\";"
     "  var script_elements = document.getElementsByTagName(\"script\");"
     "  for (var i = 0; i < script_elements.length; ++i) {"
     "    var script_html = script_elements[i].innerHTML;"
     "    if (script_html.indexOf(start_tag) > 0)"
     "      return script_html;"
     "  }"
     "  return \"\";"
     "})()";
// Escaped encoding for GWS Voice Search service's trailing '='.
NSString* const kTrailingEqualEncoding = @"\\x3d";
// The maximum number of trailing '=' characters in a Voice Search SRP.
const NSUInteger kMaxTrailingEqualsCount = 2;

}  // namespace

NSData* ExtractVoiceSearchAudioDataFromPageHTML(NSString* page_html) {
  if (!page_html.length) {
    return nil;
  }

  // The data should be near the end of the page, so search backwards.
  NSRange data_start_tag_range = [page_html rangeOfString:kTTSStartTag
                                                  options:NSBackwardsSearch];
  if (data_start_tag_range.location == NSNotFound) {
    DLOG(ERROR) << "Did not find base tts tag in search output. "
                << page_html.length;
    return nil;
  }

  // The base64-encoded data will be everything between
  // `audioDataStartTag` and `audioDataEndTag`.
  NSUInteger start_position =
      data_start_tag_range.location + data_start_tag_range.length;
  NSRange data_range =
      NSMakeRange(start_position, page_html.length - start_position);
  NSRange data_end_tag_range = [page_html rangeOfString:kTTSEndTag
                                                options:0
                                                  range:data_range];
  if (data_end_tag_range.location == NSNotFound ||
      data_end_tag_range.location == start_position) {
    DLOG(ERROR) << "Could not find encoded data before tts closing tag.";
    return nil;
  }

  // Extract the data between the tags.
  NSRange audio_data_range =
      NSMakeRange(start_position, data_end_tag_range.location - start_position);
  NSString* raw_base64_encoded_audio_string =
      [page_html substringWithRange:audio_data_range];
  if (!raw_base64_encoded_audio_string) {
    DLOG(ERROR) << "Could not find encoded data between tags.";
    return nil;
  }

  // GWS is escaping the trailing '=' characters to \x3d.
  // Clean these up before passing the string to the base64 decoder.
  // Note: there are at most 2 encoded trailing '=' characters, so limit the
  // string replacement to the last characters of the string.
  NSUInteger search_range_length =
      std::min(kMaxTrailingEqualsCount * kTrailingEqualEncoding.length,
               raw_base64_encoded_audio_string.length);
  NSRange search_range =
      NSMakeRange(raw_base64_encoded_audio_string.length - search_range_length,
                  search_range_length);
  const std::string base64_encoded_audio_string =
      base::SysNSStringToUTF8([raw_base64_encoded_audio_string
          stringByReplacingOccurrencesOfString:kTrailingEqualEncoding
                                    withString:@"="
                                       options:0
                                         range:search_range]);

  std::string decoded_data;
  if (!base::Base64Decode(base64_encoded_audio_string, &decoded_data)) {
    return nil;
  }

  return [NSData dataWithBytes:decoded_data.c_str() length:decoded_data.size()];
}

void ExtractVoiceSearchAudioDataFromWebState(
    web::WebState* webState,
    TextToSpeechCompletion completion) {
  DCHECK(webState);
  DCHECK(completion);
  NSString* tts_extraction_script = [NSString
      stringWithFormat:kTTSAudioDataExtractorScriptFormat, kTTSStartTag];

  web::WebFrame* web_frame =
      webState->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!web_frame) {
    completion(nil);
    return;
  }

  std::u16string script = base::SysNSStringToUTF16(tts_extraction_script);
  web_frame->ExecuteJavaScript(
      script, BindOnce(^(const base::Value* value) {
        if (value->is_string()) {
          NSString* result = base::SysUTF8ToNSString(value->GetString());
          completion(ExtractVoiceSearchAudioDataFromPageHTML(result));
        }
      }));
}
