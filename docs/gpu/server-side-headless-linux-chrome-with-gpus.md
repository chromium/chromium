# Running Headless Chrome on Server-Side Linux with GPU Support

Many cloud environments offer hardware compute instances with GPU
support. It is challenging to figure out how to run a headless Chrome
instance on this hardware and properly activate the GPU.

For Linux instances with NVIDIA GPUs, there is now public
documentation on how to achieve this:

[Using headless Chrome on server side environments such as Google
Cloud Platform or Google Colab for true client side browser emulation
with NVIDIA server GPUs for Web AI or graphical workloads using WebGL
or
WebGPU.](https://github.com/jasonmayes/headless-chrome-nvidia-t4-gpu-support)

See also the [high-level blog
post](https://developer.chrome.com/blog/supercharge-web-ai-testing)
and [docs for using GPUs in a Google Colab
environment](https://developer.chrome.com/docs/web-platform/webgpu/colab-headless).
