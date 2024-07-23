## Using GPU Hardware in Headless Chrome

Headless Chrome can utilize the local machine's GPU, at least in some
circumstances. This capability is useful for Continuous Integration
setups, running web workloads server-side, and in other scenarios.

With headless Chrome, pass the command line argument `--enable-gpu` to
disable forcing software rendering. This defers to Chrome's default
OpenGL driver autodetection, which on Linux requires that X display is
available (i.e. X11 server is available and `DISPLAY` env var is set
accordingly). While the default auto-detection doesn't seem to work
without X11, forcing Vulkan backend (--use-angle=vulkan) have been
found to work at least on some Linux configurations.

Linux NVIDIA users may find [Server Side Headless Linux Chrome With
GPUs] helpful.

For additional background and information please see
[crbug.com/40540071](https://crbug.com/40540071),
[crbug.com/338414704](https://crbug.com/338414704),
[crbug.com/40256775](https://crbug.com/40256775), and
[crbug.com/40062624](https://crbug.com/40062624).

[Server Side Headless Linux Chrome With GPUs]: server-side-headless-linux-chrome-with-gpus.md
