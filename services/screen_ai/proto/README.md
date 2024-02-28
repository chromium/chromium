# Main content extraction proto convertor unittests

## Creating/updating an expected proto output.

1. Turn on `WRITE_DEBUG_PROTO` in [main_content_extractor_proto_convertor_unittest.cc](main_content_extractor_proto_convertor_unittest.cc).
2. Run its services unittests under the suite of
   *MainContentExtractorProtoConvertorTest*. An example command line is:
   ```console
   out/Debug/services_unittests --gtest_filter=MainContentExtractorProtoConvertor*
   ```
3. Run the following gqui command to convert a debug binary proto into an
   expected text proto:
   ```console
   gqui from rawproto:[DEBUG PROTO PATH] proto screenai.ViewHierarchy --noprotoprint_annotations --outfile=textproto:[OUTPUT TEXTPROTO PATH]
   ```
4. Delete the most outer curly brackets in the text proto.
5. Rename the text proto based on `kProtoConversionSampleExpectedFileNameFormat`
   in [main_content_extractor_proto_convertor_unittest.cc](main_content_extractor_proto_convertor_unittest.cc).
6. Move the text proto to [services/test/data/screen_ai](https://source.chromium.org/chromium/chromium/src/+/main:services/test/data/screen_ai/).
