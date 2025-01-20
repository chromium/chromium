# Video Effects Test Models

This directory contains test models for use with the video effects service.
They are obtained from the mediapipe repository and can be downloaded by
appending the file name to the following URL:

https://storage.googleapis.com/mediapipe-assets/

Please keep the following table updated with the version information for each
model file when updating them.

| Model                                | Modified Date |
|:-------------------------------------|:--------------|
|selfie_segmentation.tflite            | 2023-05-06    |
|selfie_segmentation_landscape.crx3    | 2023-05-06    |
|selfie_segmentation_landscape.tflite  | 2023-05-06    |

## Testing

In addition to the .tflite file, this directory contains a .crx3 file derived
from the TFLite model. This file contains the model in a format suitable for
using in Chrome. In order to override the model file to be used, the flag
`--optimization-guide-model-override` can be passed in when launching Chrome.

For example, in order to allow the Video Effects Service to use the segmentation
model contained in this directory, run Chrome as follows:

```
src> outdir/chrome --optimization-guide-model-override=OPTIMIZATION_TARGET_CAMERA_BACKGROUND_SEGMENTATION|services/test/data/video_effects/models/selfie_segmentation_landscape.crx3
```

In cmd.exe on Windows, the pipe (`|`) character is reserved and must be escaped
by using `^|` character sequence. When running on Windows, use the following:

```
src> outdir\chrome.exe --optimization-guide-model-override=OPTIMIZATION_TARGET_CAMERA_BACKGROUND_SEGMENTATION^|services\test\data\video_effects\models\selfie_segmentation_landscape.crx3
```
