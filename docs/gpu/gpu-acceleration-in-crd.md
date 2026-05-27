## Using GPU Acceleration in Chrome Remote Desktop

When Chrome Remote Desktop is launched in Wayland mode, it can easily
provide GPU acceleration for Chrome instances launched within it!
Thanks to colleagues on the Chrome Remote Desktop team for
demonstrating how to get this working in internal Bug 511233442.

In short, on a Google-managed Linux workstation:

* Enable CRD Wayland mode ([go/crd-wayland](http://go/crd-wayland))
* Add your user-account to the "render" group (`sudo usermod -a -G render $USER`)
* Reboot (just to make sure everything takes effect)

Chrome running in Wayland mode receives acceleration via the host
machine's GPU!
