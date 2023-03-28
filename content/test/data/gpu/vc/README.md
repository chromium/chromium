# GPU Demo for Video Conferencing Simulation

The demo files display MxN videos with a small rectangle (UI) on top of each
video, and 2 FPS panels to respectively indicate the whole page and the average
video refreshment rates. A small video simulating the image from the local
camera is displayed at the top right corner.


## Configurations

The on-screen size that covers all videos is set to 1600x900. The actual size of
each video is divided by the video rows and columns (MxN).

The default video rows and columns is 7x7 which can be changed from the URL
string.

The video stream size is 320x280 if the total video count is more than 4. Among
these videos, 4 are 30 fps, 12 are 15 fps, and the remaining are 7 fps.
The video stream size is 640x360 at 30 fps if the total video count is equal or
less than 4.

All default videos are VP9 coding format. It can be changed to VP8.

### videos_mxn.html
Video elements are played through the default web media player. The UI are added
with CSS icons.

### webgl_videos_mxn.html
### webgl2_videos_mxn.html
  PLEASE RUN http-server TO SERVE THIS DEMO, OTHERWISE THIS DEMO WILL NOT START.
The image of each video frame is uploaded and rendered by WebGL. The UI is also
rendered by WebGL. The demo uses texImage2D API to copy the video textures into GPU.

### webgpu_videos_mxn.html
  PLEASE RUN http-server TO SERVE THIS DEMO, OTHERWISE THIS DEMO WILL NOT START.
Chromium command line switch to enable WEBGPU: `--enable-unsafe-webgpu`
The image of each video frame is uploaded and rendered by WebGPU. The UI is also
rendered by WebGPU. The demo uses importExternalTexture API to copy the video
textures into GPU. The copy method can be changed to createImageBitmap() then
device.queue.copyExternalImageToTexture() with a flag.

## Usage

To change the numbers of videos. Use `rows=m` and `columns=n`.
```
videos_mxn.html?rows=4&columns=7
```
```
webgpu_videos_mxn.html?rows=2&columns=2
```

To force VP8 video sources. Use `codec=vp8`.
```
videos_mxn.html?codec=vp8
```
```
webgpu_videos_mxn.html?codec=vp8
```

For webgl_videos_mxn.html, webgl2_videos_mxn.html and webgpu_videos_mxn.html:
To remove the UI icons. Use `ui=none`.
```
webgpu_videos_mxn.html?ui=none
```

To remove the FPS panels. Use `fps=none`.
```
webgpu_videos_mxn.html?fps=none
```

To use largest video only. Use `use_large_size_video=1`
```
webgpu_videos_mxn.html?use_large_size_video=1
```

To disable the 30fps cap in the sample. Use `cap_ui_fps=0`
```
webgpu_videos_mxn.html?cap_ui_fps=0
```

To show a local webcam stream.  Use `use_local_camera=1`
```
webgpu_videos_mxn.html?use_local_camera=1
```

For webgl2_videos_mxn.html only:
To use fixed texture size via texStorage2D, use `fixed_texture_size=1`.
```
webgl2_videos_mxn.html?fixed_texture_size=1
```

For webgpu_videos_mxn.html only:
To disable Import Texture API and force the video texture copy through
createImageBitmap() and then copyExternalImageToTexture(),
use `import_texture_api=0`.
```
webgpu_videos_mxn.html?import_texture_api=0
```

To disable using onSubmittedWorkDone to workaround webgpu
backpressure issue(may hurt performance). Use `back_pressure_workaround=0`
```
webgpu_videos_mxn.html?back_pressure_workaround=0
```


## Video Files

The following videos are recorded and converted to the needed configurations
by magchen@chromium.org.

### VP9
#### teddy1_vp9_640x360_30fps.webm
ffmpeg -i Teddy1_hd.MOV -c:v libvpx-vp9 -r 30 -s 640x360 teddy1_vp9_640x360_30fps.webm

#### teddy1_vp9_320x180_7fps.webm
ffmpeg -i Teddy1_hd.MOV -c:v libvpx-vp9 -r 7 -s 320x180 teddy1_vp9_320x180_7fps.webm

#### teddy2_vp9_320x180_15fps.webm
ffmpeg -i Teddy2_hd.MOV -c:v libvpx-vp9 -r 15 -s 320x180 teddy2_vp9_320x180_15fps.webm

#### teddy3_vp9_320x180_30fps.webm
ffmpeg -i Teddy3_hd.MOV -c:v libvpx-vp9 -r 30 -s 320x180 teddy3_vp9_320x180_30fps.webm

### VP8
#### teddy1_vp8_640x360_30fps.webm
ffmpeg -i Teddy1_hd.MOV -c:v vp8 -r 30 -s 640x360 teddy1_vp8_640x360_30fps.webm

#### teddy1_vp8_320x180_7fps.webm
ffmpeg -i Teddy1_hd.MOV -c:v vp8 -r 7 -s 320x180 teddy1_vp8_320x180_7fps.webm

#### teddy2_vp8_320x180_15fps.webm
ffmpeg -i Teddy2_hd.MOV -c:v vp8 -r 15 -s 320x180 teddy2_vp8_320x180_15fps.webm

#### teddy3_vp8_320x180_30fps.webm
ffmpeg -i Teddy3_hd.MOV -c:v vp8 -r 30 -s 320x180 teddy3_vp8_320x180_30fps.webm
