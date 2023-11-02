# Translation Screenshots

We’d all like to ensure that the user experience in Chrome is optimal, no matter the locale. Translation Screenshots strives to achieve this. The goal is to improve translations of Chrome’s UI strings by providing more and clearer context for our translators. Bad localizations cause us to lose user trust and are expensive to fix, in both time and resources.

## How to add a translation screenshot

1. Take a screenshot of the UI that displays the modified string (e.g. `IDS_NEW_STRING` in `path/to/file.grd`)
2. Save it under the screenshot directory for the file you are editing: `path/to/file_grd/IDS_NEW_STRING.png`. If `path/to/file_grd` doesn’t exist, create it. Only .png files are supported.
3. Run `python3 tools/translation/upload_screenshots.py` (without flags) to upload the screenshot to Google Cloud Storage. (You may need to use `python` instead of `python3` depending on your system)
4. This will generate `path/to/file_grd/IDS_NEW_STRING.png.sha1`. Add this file to your CL and you are done. Don’t add the actual image to your CL.

Unfortunately, only contributors with @google.com accounts can upload images to the Cloud Storage bucket (Step 3). If you can’t upload images, please ask a Googler for help, then continue with Step 4.


See [Instructions & FAQ](https://docs.google.com/document/d/1nwYWDny20icMSpLUuV_LgrlbWKrYpbXOERUIZNH636o/) for details. And here is the [original announcement](https://groups.google.com/a/chromium.org/forum/#!msg/chromium-dev/6kcVb-eFUg8/qHuUnbJ7BgAJ) of this feature.
