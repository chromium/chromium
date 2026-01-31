# No VM Sandboxes

**As of: 2025-12-17**

Every so often, someone has the following idea:

"Instead of sandboxing processes using OS primitives, what if we ran them in a
virtual machine and used the VM as the isolation layer?"

This doc outlines why we don't do that. It's written in terms of "the renderer"
since this is the process people usually are talking about sandboxing this way,
but the same rationales apply to the other processes we create.

## Porting the Renderer

The renderer depends on a lot of OS primitives. At a minimum, to work it
_requires_:

* Memory allocation (including mapping executable memory)
* Concurrency primitives (threads, sync primitives, etc)
* I/O sufficiently fast to move frames worth of content in and out
* Access to various platform data sources (fonts, icons, and the like)

The renderer currently runs on several operating systems, and we have
implementations of all these primitives for those operating systems. If we were
to run the renderer inside a VM, we'd need to either:

* Boot an existing OS the renderer runs on in the VM, or
* Boot an existing OS the renderer doesn't currently run on in the VM, or
* Write a new OS to provide these services to the renderer in the VM, or
* Port the renderer to the bare "hardware" interface the VM provides

All of these are basically impractical:

* Booting an existing OS the renderer already supports doesn't necessarily
  require porting, means that at a minimum we would pay the memory overhead
  of an entire OS kernel. This footprint is in the multiple megabytes even for
  a stripped-down Linux kernel.
* Porting-based approaches mean we'd be reimplementing a lot of OS primitives.
  These are often heavily optimized on existing systems and deeply integrated -
  for example, the OS primitive mutex implementation may well be integrated with
  the OS scheduler. We would need to figure out how to make this work correctly
  (if it was possible) between guest and host, even if the guest and OS are
  quite different OSes.

## I/O

A single graphical frame at 4K resolution is about 30MB of pixel data,
uncompressed. Delivering 4K at 60hz requires the renderer to produce over 1GB of
data per second and deliver it to the host GPU. This essentially forces shared
guest/host memory mappings to retain acceptable performance, and virtually
mandates that data passes directly to the GPU without being copied or
expensively validated. This isn't impossible, but it constrains the design of
the VM and requires sharing of host devices directly with the guest.
[Sommelier] addresses some of these problems, but only for one host (ChromeOS).

The challenges of (for example) having a guest running on Linux use a graphics
stack hosted on a Windows device in a high-performance way are considerable, and
solving those challenges could add a serious amount of complexity to Chromium.

## Performance

Hot areas of the renderer are extremely performance sensitive, and penalties of
1-2% of performance may already be too much for us to pay. Since at a _minimum_
VMs have to trap and emulate not only system calls but some machine
instructions, this performance target may be difficult to hit. Even with
paravirtualization, there are likely to be difficult performance problems to
resolve.

Separately from that, some areas of the renderer require access to relatively
esoteric CPU features for performance reasons. If those are not implemented in
whichever VM monitor we use or write, we would need to add them, or otherwise
figure out how to deal with the performance hit. This could include things like:

* Vector / SIMD instructions or AVX
* Crypto instructions like AESNI
* Specialized floating-point instructions

Some I/O paths into and out of the renderer are also on the hot path for page
loads, which is a metric that we optimize for heavily. Adding additional hops to
this path, and especially extra context switches or hypervisor entries / exits,
is likely to add intolerable amounts of latency here.

## Debugging & Crash Reporting

If the renderer is inside a VM, crashes that happen inside the VM somehow need
to be exposed to the host so they can be uploaded to the crash reporting
service. Also, developers need to somehow be able to attach debuggers and use
tools like ASAN on renderers running in this manner, which will probably involve
building a remote-debug setup.

Some mechanisms, like HWAsan, are not likely to work at all inside a VM.

## Security Implications

One of the promising areas for security investment is control-flow integrity.
There are many technologies for doing this; all of them require hardware support
and none of them are likely to work well from within a VM. Whichever VM we
choose or build will need to understand CFI, as applied to the renderer running
under it, well enough to integrate with the host's CFI primitives. This could be
extremely challenging or even impossible depending on the host and guest OSes
and hardware.

Therefore, moving to a VM-based sandbox would likely prevent deployment of CFI,
or at least make it considerably more difficult. There are ongoing efforts
currently to support CET (one such technology from Intel) inside VM guests.

Additionally, while the VM sandbox would add a layer of isolation between the
renderer and the host kernel, it adds new attack surface we have to secure: the
VM hypervisor itself. That hypervisor will need to be relatively full-featured
to deliver the primitives the renderer needs to function with acceptable
performance, which means it is likely to be a large body of code with security
bugs of its own.

[Sommelier]: https://chromium.googlesource.com/chromiumos/platform2/+/master/vm_tools/sommelier/
