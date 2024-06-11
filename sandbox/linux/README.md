# The Linux Sandbox

The Linux sandbox provides an API for restricting the capabilities of a process.
The overall design philosophy of the sandbox is documented
[elsewhere](../docs/design/sandbox.md); this document explains how it works on
Linux.

## Overall Design

There are several different sandboxing mechanisms available on Linux:

* setuid(2)
* namespaces
* seccomp(2) BPF
* seccomp(2) legacy
* selinux(7)
* apparmor(7)
* landlock(7)

Chromium chooses which mechanisms to use based on which kernel features are
available. We also generally use multiple layers of sandboxing, to achieve both
confinement for the process and reduction of the exposed kernel attack surface.
Of these mechanisms, Chrome uses:

* setuid(2) everywhere
* namespaces where supported (modern Linux kernels)
* seccomp(2) BPF where supported (modern Linux kernels)

And we used to use, but no longer use:

* selinux(7)
* apparmor(7)

## setuid(2)

The setuid(2) sandbox takes advantage of the fact that privileged processes on
Linux are allowed to create new namespaces (see namespaces(7)) and sandboxes the
renderer by creating empty namespaces for it at launch time. It relies on a
setuid binary, usually installed at `/opt/google/chrome/chrome-sandbox`, which:

* Enters new PID and network namespaces, preventing the sandboxed process from
  directly accessing the network or seeing any other processes.
* chroot()s into a "safe" directory (currently inside the process's own /proc
  directory) by spawning a privileged helper process which shares its fs state
  (using `CLONE_FS`) and having that helper chroot() it, which leaves the
  process in an empty, readonly root directory.
* Marks itself as un-dumpable using `prctl(2)`, which prevents any process
  without `CAP_SYS_PTRACE` from tracing it. In theory this would keep renderers
  from debugging each other, but in practice they are isolated from each other
  by PID namespaces anyway.
* Uses capset(2) to drop all inherited capabilities.
* Drops from root back to the uid/gid/etc of the user running the browser

In general, the setuid sandbox makes an effort to apply all these mitigations,
but support for them varies between kernel versions, so the strength of the
setuid sandbox is variable, with newer kernels providing better security.

The setuid sandbox is implemented in [suid/](suid/).

If you need to disable it, you can use `--disable-setuid-sandbox`. You should
also see
[docs/linux/suid_sandbox_development.md](../../docs/linux/suid_sandbox_development.md)
for advice on developing the setuid sandbox itself.

## seccomp(2) BPF

On modern Linuxes, we use the filter mode of seccomp(2), which allows us to
supply a program (written in a domain-specific language called "BPF", see bpf(2)
and bpfc(1)) which is evaluated every time the sandboxed process makes a syscall
to figure out whether the syscall should be allowed. The seccomp filters are
compiled and applied "early" in the syscall process, so this both constrains
what the process can do and reduces attack surface of the kernel.

The seccomp sandbox is implemented in [seccomp-bpf/](seccomp-bpf/), and our
tools for working with the BPF DSL are in [bpf_dsl/](bpf_dsl/). The actual
baseline policies we use are in [seccomp-bpf-helpers/](seccomp-bpf-helpers/).

Since the seccomp sandbox has a filter that is applied to all syscalls being
made, to use it you must have an exhaustive list of syscalls that could be made
by the code being sandboxed - both code you did write and code you didn't write.
Generating that list of syscalls can be difficult and so it is helpful to have
very good test coverage **which runs under the sandbox** to ensure you are
exercising any code paths that could lead to syscalls.

## landlock(7)

We currently don't use Landlock, but we'd like to:
[345514921](https://issues.chromium.org/issues/345514921).
