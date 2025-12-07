// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generated from the Linux kernel's calls.S.
#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_ARM_LINUX_SYSCALLS_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_ARM_LINUX_SYSCALLS_H_

#if !defined(__arm__) || !defined(__ARM_EABI__)
#error "Including header on wrong architecture"
#endif

#if !defined(__NR_SYSCALL_BASE)
// On ARM EABI arch, __NR_SYSCALL_BASE is 0.
#define __NR_SYSCALL_BASE 0
#endif

// This syscall list has holes, because ARM EABI makes some syscalls obsolete.

#if !defined(__NR_restart_syscall)
#define __NR_restart_syscall (__NR_SYSCALL_BASE+0)
#endif

#if !defined(__NR_exit)
#define __NR_exit (__NR_SYSCALL_BASE+1)
#endif

#if !defined(__NR_fork)
#define __NR_fork (__NR_SYSCALL_BASE+2)
#endif

#if !defined(__NR_read)
#define __NR_read (__NR_SYSCALL_BASE+3)
#endif

#if !defined(__NR_write)
#define __NR_write (__NR_SYSCALL_BASE+4)
#endif

#if !defined(__NR_open)
#define __NR_open (__NR_SYSCALL_BASE+5)
#endif

#if !defined(__NR_close)
#define __NR_close (__NR_SYSCALL_BASE+6)
#endif

#if !defined(__NR_creat)
#define __NR_creat (__NR_SYSCALL_BASE+8)
#endif

#if !defined(__NR_link)
#define __NR_link (__NR_SYSCALL_BASE+9)
#endif

#if !defined(__NR_unlink)
#define __NR_unlink (__NR_SYSCALL_BASE+10)
#endif

#if !defined(__NR_execve)
#define __NR_execve (__NR_SYSCALL_BASE+11)
#endif

#if !defined(__NR_chdir)
#define __NR_chdir (__NR_SYSCALL_BASE+12)
#endif

#if !defined(__NR_mknod)
#define __NR_mknod (__NR_SYSCALL_BASE+14)
#endif

#if !defined(__NR_chmod)
#define __NR_chmod (__NR_SYSCALL_BASE+15)
#endif

#if !defined(__NR_lchown)
#define __NR_lchown (__NR_SYSCALL_BASE+16)
#endif

#if !defined(__NR_lseek)
#define __NR_lseek (__NR_SYSCALL_BASE+19)
#endif

#if !defined(__NR_getpid)
#define __NR_getpid (__NR_SYSCALL_BASE+20)
#endif

#if !defined(__NR_mount)
#define __NR_mount (__NR_SYSCALL_BASE+21)
#endif

#if !defined(__NR_setuid)
#define __NR_setuid (__NR_SYSCALL_BASE+23)
#endif

#if !defined(__NR_getuid)
#define __NR_getuid (__NR_SYSCALL_BASE+24)
#endif

#if !defined(__NR_ptrace)
#define __NR_ptrace (__NR_SYSCALL_BASE+26)
#endif

#if !defined(__NR_pause)
#define __NR_pause (__NR_SYSCALL_BASE+29)
#endif

#if !defined(__NR_access)
#define __NR_access (__NR_SYSCALL_BASE+33)
#endif

#if !defined(__NR_nice)
#define __NR_nice (__NR_SYSCALL_BASE+34)
#endif

#if !defined(__NR_sync)
#define __NR_sync (__NR_SYSCALL_BASE+36)
#endif

#if !defined(__NR_kill)
#define __NR_kill (__NR_SYSCALL_BASE+37)
#endif

#if !defined(__NR_rename)
#define __NR_rename (__NR_SYSCALL_BASE+38)
#endif

#if !defined(__NR_mkdir)
#define __NR_mkdir (__NR_SYSCALL_BASE+39)
#endif

#if !defined(__NR_rmdir)
#define __NR_rmdir (__NR_SYSCALL_BASE+40)
#endif

#if !defined(__NR_dup)
#define __NR_dup (__NR_SYSCALL_BASE+41)
#endif

#if !defined(__NR_pipe)
#define __NR_pipe (__NR_SYSCALL_BASE+42)
#endif

#if !defined(__NR_times)
#define __NR_times (__NR_SYSCALL_BASE+43)
#endif

#if !defined(__NR_brk)
#define __NR_brk (__NR_SYSCALL_BASE+45)
#endif

#if !defined(__NR_setgid)
#define __NR_setgid (__NR_SYSCALL_BASE+46)
#endif

#if !defined(__NR_getgid)
#define __NR_getgid (__NR_SYSCALL_BASE+47)
#endif

#if !defined(__NR_geteuid)
#define __NR_geteuid (__NR_SYSCALL_BASE+49)
#endif

#if !defined(__NR_getegid)
#define __NR_getegid (__NR_SYSCALL_BASE+50)
#endif

#if !defined(__NR_acct)
#define __NR_acct (__NR_SYSCALL_BASE+51)
#endif

#if !defined(__NR_umount2)
#define __NR_umount2 (__NR_SYSCALL_BASE+52)
#endif

#if !defined(__NR_ioctl)
#define __NR_ioctl (__NR_SYSCALL_BASE+54)
#endif

#if !defined(__NR_fcntl)
#define __NR_fcntl (__NR_SYSCALL_BASE+55)
#endif

#if !defined(__NR_setpgid)
#define __NR_setpgid (__NR_SYSCALL_BASE+57)
#endif

#if !defined(__NR_umask)
#define __NR_umask (__NR_SYSCALL_BASE+60)
#endif

#if !defined(__NR_chroot)
#define __NR_chroot (__NR_SYSCALL_BASE+61)
#endif

#if !defined(__NR_ustat)
#define __NR_ustat (__NR_SYSCALL_BASE+62)
#endif

#if !defined(__NR_dup2)
#define __NR_dup2 (__NR_SYSCALL_BASE+63)
#endif

#if !defined(__NR_getppid)
#define __NR_getppid (__NR_SYSCALL_BASE+64)
#endif

#if !defined(__NR_getpgrp)
#define __NR_getpgrp (__NR_SYSCALL_BASE+65)
#endif

#if !defined(__NR_setsid)
#define __NR_setsid (__NR_SYSCALL_BASE+66)
#endif

#if !defined(__NR_sigaction)
#define __NR_sigaction (__NR_SYSCALL_BASE+67)
#endif

#if !defined(__NR_setreuid)
#define __NR_setreuid (__NR_SYSCALL_BASE+70)
#endif

#if !defined(__NR_setregid)
#define __NR_setregid (__NR_SYSCALL_BASE+71)
#endif

#if !defined(__NR_sigsuspend)
#define __NR_sigsuspend (__NR_SYSCALL_BASE+72)
#endif

#if !defined(__NR_sigpending)
#define __NR_sigpending (__NR_SYSCALL_BASE+73)
#endif

#if !defined(__NR_sethostname)
#define __NR_sethostname (__NR_SYSCALL_BASE+74)
#endif

#if !defined(__NR_setrlimit)
#define __NR_setrlimit (__NR_SYSCALL_BASE+75)
#endif

#if !defined(__NR_getrusage)
#define __NR_getrusage (__NR_SYSCALL_BASE+77)
#endif

#if !defined(__NR_gettimeofday)
#define __NR_gettimeofday (__NR_SYSCALL_BASE+78)
#endif

#if !defined(__NR_settimeofday)
#define __NR_settimeofday (__NR_SYSCALL_BASE+79)
#endif

#if !defined(__NR_getgroups)
#define __NR_getgroups (__NR_SYSCALL_BASE+80)
#endif

#if !defined(__NR_setgroups)
#define __NR_setgroups (__NR_SYSCALL_BASE+81)
#endif

#if !defined(__NR_symlink)
#define __NR_symlink (__NR_SYSCALL_BASE+83)
#endif

#if !defined(__NR_readlink)
#define __NR_readlink (__NR_SYSCALL_BASE+85)
#endif

#if !defined(__NR_uselib)
#define __NR_uselib (__NR_SYSCALL_BASE+86)
#endif

#if !defined(__NR_swapon)
#define __NR_swapon (__NR_SYSCALL_BASE+87)
#endif

#if !defined(__NR_reboot)
#define __NR_reboot (__NR_SYSCALL_BASE+88)
#endif

#if !defined(__NR_munmap)
#define __NR_munmap (__NR_SYSCALL_BASE+91)
#endif

#if !defined(__NR_truncate)
#define __NR_truncate (__NR_SYSCALL_BASE+92)
#endif

#if !defined(__NR_ftruncate)
#define __NR_ftruncate (__NR_SYSCALL_BASE+93)
#endif

#if !defined(__NR_fchmod)
#define __NR_fchmod (__NR_SYSCALL_BASE+94)
#endif

#if !defined(__NR_fchown)
#define __NR_fchown (__NR_SYSCALL_BASE+95)
#endif

#if !defined(__NR_getpriority)
#define __NR_getpriority (__NR_SYSCALL_BASE+96)
#endif

#if !defined(__NR_setpriority)
#define __NR_setpriority (__NR_SYSCALL_BASE+97)
#endif

#if !defined(__NR_statfs)
#define __NR_statfs (__NR_SYSCALL_BASE+99)
#endif

#if !defined(__NR_fstatfs)
#define __NR_fstatfs (__NR_SYSCALL_BASE+100)
#endif

#if !defined(__NR_syslog)
#define __NR_syslog (__NR_SYSCALL_BASE+103)
#endif

#if !defined(__NR_setitimer)
#define __NR_setitimer (__NR_SYSCALL_BASE+104)
#endif

#if !defined(__NR_getitimer)
#define __NR_getitimer (__NR_SYSCALL_BASE+105)
#endif

#if !defined(__NR_stat)
#define __NR_stat (__NR_SYSCALL_BASE+106)
#endif

#if !defined(__NR_lstat)
#define __NR_lstat (__NR_SYSCALL_BASE+107)
#endif

#if !defined(__NR_fstat)
#define __NR_fstat (__NR_SYSCALL_BASE+108)
#endif

#if !defined(__NR_vhangup)
#define __NR_vhangup (__NR_SYSCALL_BASE+111)
#endif

#if !defined(__NR_wait4)
#define __NR_wait4 (__NR_SYSCALL_BASE+114)
#endif

#if !defined(__NR_swapoff)
#define __NR_swapoff (__NR_SYSCALL_BASE+115)
#endif

#if !defined(__NR_sysinfo)
#define __NR_sysinfo (__NR_SYSCALL_BASE+116)
#endif

#if !defined(__NR_fsync)
#define __NR_fsync (__NR_SYSCALL_BASE+118)
#endif

#if !defined(__NR_sigreturn)
#define __NR_sigreturn (__NR_SYSCALL_BASE+119)
#endif

#if !defined(__NR_clone)
#define __NR_clone (__NR_SYSCALL_BASE+120)
#endif

#if !defined(__NR_setdomainname)
#define __NR_setdomainname (__NR_SYSCALL_BASE+121)
#endif

#if !defined(__NR_uname)
#define __NR_uname (__NR_SYSCALL_BASE+122)
#endif

#if !defined(__NR_adjtimex)
#define __NR_adjtimex (__NR_SYSCALL_BASE+124)
#endif

#if !defined(__NR_mprotect)
#define __NR_mprotect (__NR_SYSCALL_BASE+125)
#endif

#if !defined(__NR_sigprocmask)
#define __NR_sigprocmask (__NR_SYSCALL_BASE+126)
#endif

#if !defined(__NR_init_module)
#define __NR_init_module (__NR_SYSCALL_BASE+128)
#endif

#if !defined(__NR_delete_module)
#define __NR_delete_module (__NR_SYSCALL_BASE+129)
#endif

#if !defined(__NR_quotactl)
#define __NR_quotactl (__NR_SYSCALL_BASE+131)
#endif

#if !defined(__NR_getpgid)
#define __NR_getpgid (__NR_SYSCALL_BASE+132)
#endif

#if !defined(__NR_fchdir)
#define __NR_fchdir (__NR_SYSCALL_BASE+133)
#endif

#if !defined(__NR_bdflush)
#define __NR_bdflush (__NR_SYSCALL_BASE+134)
#endif

#if !defined(__NR_sysfs)
#define __NR_sysfs (__NR_SYSCALL_BASE+135)
#endif

#if !defined(__NR_personality)
#define __NR_personality (__NR_SYSCALL_BASE+136)
#endif

#if !defined(__NR_setfsuid)
#define __NR_setfsuid (__NR_SYSCALL_BASE+138)
#endif

#if !defined(__NR_setfsgid)
#define __NR_setfsgid (__NR_SYSCALL_BASE+139)
#endif

#if !defined(__NR__llseek)
#define __NR__llseek (__NR_SYSCALL_BASE+140)
#endif

#if !defined(__NR_getdents)
#define __NR_getdents (__NR_SYSCALL_BASE+141)
#endif

#if !defined(__NR__newselect)
#define __NR__newselect (__NR_SYSCALL_BASE+142)
#endif

#if !defined(__NR_flock)
#define __NR_flock (__NR_SYSCALL_BASE+143)
#endif

#if !defined(__NR_msync)
#define __NR_msync (__NR_SYSCALL_BASE+144)
#endif

#if !defined(__NR_readv)
#define __NR_readv (__NR_SYSCALL_BASE+145)
#endif

#if !defined(__NR_writev)
#define __NR_writev (__NR_SYSCALL_BASE+146)
#endif

#if !defined(__NR_getsid)
#define __NR_getsid (__NR_SYSCALL_BASE+147)
#endif

#if !defined(__NR_fdatasync)
#define __NR_fdatasync (__NR_SYSCALL_BASE+148)
#endif

#if !defined(__NR__sysctl)
#define __NR__sysctl (__NR_SYSCALL_BASE+149)
#endif

#if !defined(__NR_mlock)
#define __NR_mlock (__NR_SYSCALL_BASE+150)
#endif

#if !defined(__NR_munlock)
#define __NR_munlock (__NR_SYSCALL_BASE+151)
#endif

#if !defined(__NR_mlockall)
#define __NR_mlockall (__NR_SYSCALL_BASE+152)
#endif

#if !defined(__NR_munlockall)
#define __NR_munlockall (__NR_SYSCALL_BASE+153)
#endif

#if !defined(__NR_sched_setparam)
#define __NR_sched_setparam (__NR_SYSCALL_BASE+154)
#endif

#if !defined(__NR_sched_getparam)
#define __NR_sched_getparam (__NR_SYSCALL_BASE+155)
#endif

#if !defined(__NR_sched_setscheduler)
#define __NR_sched_setscheduler (__NR_SYSCALL_BASE+156)
#endif

#if !defined(__NR_sched_getscheduler)
#define __NR_sched_getscheduler (__NR_SYSCALL_BASE+157)
#endif

#if !defined(__NR_sched_yield)
#define __NR_sched_yield (__NR_SYSCALL_BASE+158)
#endif

#if !defined(__NR_sched_get_priority_max)
#define __NR_sched_get_priority_max (__NR_SYSCALL_BASE+159)
#endif

#if !defined(__NR_sched_get_priority_min)
#define __NR_sched_get_priority_min (__NR_SYSCALL_BASE+160)
#endif

#if !defined(__NR_sched_rr_get_interval)
#define __NR_sched_rr_get_interval (__NR_SYSCALL_BASE+161)
#endif

#if !defined(__NR_nanosleep)
#define __NR_nanosleep (__NR_SYSCALL_BASE+162)
#endif

#if !defined(__NR_mremap)
#define __NR_mremap (__NR_SYSCALL_BASE+163)
#endif

#if !defined(__NR_setresuid)
#define __NR_setresuid (__NR_SYSCALL_BASE+164)
#endif

#if !defined(__NR_getresuid)
#define __NR_getresuid (__NR_SYSCALL_BASE+165)
#endif

#if !defined(__NR_poll)
#define __NR_poll (__NR_SYSCALL_BASE+168)
#endif

#if !defined(__NR_nfsservctl)
#define __NR_nfsservctl (__NR_SYSCALL_BASE+169)
#endif

#if !defined(__NR_setresgid)
#define __NR_setresgid (__NR_SYSCALL_BASE+170)
#endif

#if !defined(__NR_getresgid)
#define __NR_getresgid (__NR_SYSCALL_BASE+171)
#endif

#if !defined(__NR_prctl)
#define __NR_prctl (__NR_SYSCALL_BASE+172)
#endif

#if !defined(__NR_rt_sigreturn)
#define __NR_rt_sigreturn (__NR_SYSCALL_BASE+173)
#endif

#if !defined(__NR_rt_sigaction)
#define __NR_rt_sigaction (__NR_SYSCALL_BASE+174)
#endif

#if !defined(__NR_rt_sigprocmask)
#define __NR_rt_sigprocmask (__NR_SYSCALL_BASE+175)
#endif

#if !defined(__NR_rt_sigpending)
#define __NR_rt_sigpending (__NR_SYSCALL_BASE+176)
#endif

#if !defined(__NR_rt_sigtimedwait)
#define __NR_rt_sigtimedwait (__NR_SYSCALL_BASE+177)
#endif

#if !defined(__NR_rt_sigqueueinfo)
#define __NR_rt_sigqueueinfo (__NR_SYSCALL_BASE+178)
#endif

#if !defined(__NR_rt_sigsuspend)
#define __NR_rt_sigsuspend (__NR_SYSCALL_BASE+179)
#endif

#if !defined(__NR_pread64)
#define __NR_pread64 (__NR_SYSCALL_BASE+180)
#endif

#if !defined(__NR_pwrite64)
#define __NR_pwrite64 (__NR_SYSCALL_BASE+181)
#endif

#if !defined(__NR_chown)
#define __NR_chown (__NR_SYSCALL_BASE+182)
#endif

#if !defined(__NR_getcwd)
#define __NR_getcwd (__NR_SYSCALL_BASE+183)
#endif

#if !defined(__NR_capget)
#define __NR_capget (__NR_SYSCALL_BASE+184)
#endif

#if !defined(__NR_capset)
#define __NR_capset (__NR_SYSCALL_BASE+185)
#endif

#if !defined(__NR_sigaltstack)
#define __NR_sigaltstack (__NR_SYSCALL_BASE+186)
#endif

#if !defined(__NR_sendfile)
#define __NR_sendfile (__NR_SYSCALL_BASE+187)
#endif

#if !defined(__NR_vfork)
#define __NR_vfork (__NR_SYSCALL_BASE+190)
#endif

#if !defined(__NR_ugetrlimit)
#define __NR_ugetrlimit (__NR_SYSCALL_BASE+191)
#endif

#if !defined(__NR_mmap2)
#define __NR_mmap2 (__NR_SYSCALL_BASE+192)
#endif

#if !defined(__NR_truncate64)
#define __NR_truncate64 (__NR_SYSCALL_BASE+193)
#endif

#if !defined(__NR_ftruncate64)
#define __NR_ftruncate64 (__NR_SYSCALL_BASE+194)
#endif

#if !defined(__NR_stat64)
#define __NR_stat64 (__NR_SYSCALL_BASE+195)
#endif

#if !defined(__NR_lstat64)
#define __NR_lstat64 (__NR_SYSCALL_BASE+196)
#endif

#if !defined(__NR_fstat64)
#define __NR_fstat64 (__NR_SYSCALL_BASE+197)
#endif

#if !defined(__NR_lchown32)
#define __NR_lchown32 (__NR_SYSCALL_BASE+198)
#endif

#if !defined(__NR_getuid32)
#define __NR_getuid32 (__NR_SYSCALL_BASE+199)
#endif

#if !defined(__NR_getgid32)
#define __NR_getgid32 (__NR_SYSCALL_BASE+200)
#endif

#if !defined(__NR_geteuid32)
#define __NR_geteuid32 (__NR_SYSCALL_BASE+201)
#endif

#if !defined(__NR_getegid32)
#define __NR_getegid32 (__NR_SYSCALL_BASE+202)
#endif

#if !defined(__NR_setreuid32)
#define __NR_setreuid32 (__NR_SYSCALL_BASE+203)
#endif

#if !defined(__NR_setregid32)
#define __NR_setregid32 (__NR_SYSCALL_BASE+204)
#endif

#if !defined(__NR_getgroups32)
#define __NR_getgroups32 (__NR_SYSCALL_BASE+205)
#endif

#if !defined(__NR_setgroups32)
#define __NR_setgroups32 (__NR_SYSCALL_BASE+206)
#endif

#if !defined(__NR_fchown32)
#define __NR_fchown32 (__NR_SYSCALL_BASE+207)
#endif

#if !defined(__NR_setresuid32)
#define __NR_setresuid32 (__NR_SYSCALL_BASE+208)
#endif

#if !defined(__NR_getresuid32)
#define __NR_getresuid32 (__NR_SYSCALL_BASE+209)
#endif

#if !defined(__NR_setresgid32)
#define __NR_setresgid32 (__NR_SYSCALL_BASE+210)
#endif

#if !defined(__NR_getresgid32)
#define __NR_getresgid32 (__NR_SYSCALL_BASE+211)
#endif

#if !defined(__NR_chown32)
#define __NR_chown32 (__NR_SYSCALL_BASE+212)
#endif

#if !defined(__NR_setuid32)
#define __NR_setuid32 (__NR_SYSCALL_BASE+213)
#endif

#if !defined(__NR_setgid32)
#define __NR_setgid32 (__NR_SYSCALL_BASE+214)
#endif

#if !defined(__NR_setfsuid32)
#define __NR_setfsuid32 (__NR_SYSCALL_BASE+215)
#endif

#if !defined(__NR_setfsgid32)
#define __NR_setfsgid32 (__NR_SYSCALL_BASE+216)
#endif

#if !defined(__NR_getdents64)
#define __NR_getdents64 (__NR_SYSCALL_BASE+217)
#endif

#if !defined(__NR_pivot_root)
#define __NR_pivot_root (__NR_SYSCALL_BASE+218)
#endif

#if !defined(__NR_mincore)
#define __NR_mincore (__NR_SYSCALL_BASE+219)
#endif

#if !defined(__NR_madvise)
#define __NR_madvise (__NR_SYSCALL_BASE+220)
#endif

#if !defined(__NR_fcntl64)
#define __NR_fcntl64 (__NR_SYSCALL_BASE+221)
#endif

#if !defined(__NR_gettid)
#define __NR_gettid (__NR_SYSCALL_BASE+224)
#endif

#if !defined(__NR_readahead)
#define __NR_readahead (__NR_SYSCALL_BASE+225)
#endif

#if !defined(__NR_setxattr)
#define __NR_setxattr (__NR_SYSCALL_BASE+226)
#endif

#if !defined(__NR_lsetxattr)
#define __NR_lsetxattr (__NR_SYSCALL_BASE+227)
#endif

#if !defined(__NR_fsetxattr)
#define __NR_fsetxattr (__NR_SYSCALL_BASE+228)
#endif

#if !defined(__NR_getxattr)
#define __NR_getxattr (__NR_SYSCALL_BASE+229)
#endif

#if !defined(__NR_lgetxattr)
#define __NR_lgetxattr (__NR_SYSCALL_BASE+230)
#endif

#if !defined(__NR_fgetxattr)
#define __NR_fgetxattr (__NR_SYSCALL_BASE+231)
#endif

#if !defined(__NR_listxattr)
#define __NR_listxattr (__NR_SYSCALL_BASE+232)
#endif

#if !defined(__NR_llistxattr)
#define __NR_llistxattr (__NR_SYSCALL_BASE+233)
#endif

#if !defined(__NR_flistxattr)
#define __NR_flistxattr (__NR_SYSCALL_BASE+234)
#endif

#if !defined(__NR_removexattr)
#define __NR_removexattr (__NR_SYSCALL_BASE+235)
#endif

#if !defined(__NR_lremovexattr)
#define __NR_lremovexattr (__NR_SYSCALL_BASE+236)
#endif

#if !defined(__NR_fremovexattr)
#define __NR_fremovexattr (__NR_SYSCALL_BASE+237)
#endif

#if !defined(__NR_tkill)
#define __NR_tkill (__NR_SYSCALL_BASE+238)
#endif

#if !defined(__NR_sendfile64)
#define __NR_sendfile64 (__NR_SYSCALL_BASE+239)
#endif

#if !defined(__NR_futex)
#define __NR_futex (__NR_SYSCALL_BASE+240)
#endif

#if !defined(__NR_sched_setaffinity)
#define __NR_sched_setaffinity (__NR_SYSCALL_BASE+241)
#endif

#if !defined(__NR_sched_getaffinity)
#define __NR_sched_getaffinity (__NR_SYSCALL_BASE+242)
#endif

#if !defined(__NR_io_setup)
#define __NR_io_setup (__NR_SYSCALL_BASE+243)
#endif

#if !defined(__NR_io_destroy)
#define __NR_io_destroy (__NR_SYSCALL_BASE+244)
#endif

#if !defined(__NR_io_getevents)
#define __NR_io_getevents (__NR_SYSCALL_BASE+245)
#endif

#if !defined(__NR_io_submit)
#define __NR_io_submit (__NR_SYSCALL_BASE+246)
#endif

#if !defined(__NR_io_cancel)
#define __NR_io_cancel (__NR_SYSCALL_BASE+247)
#endif

#if !defined(__NR_exit_group)
#define __NR_exit_group (__NR_SYSCALL_BASE+248)
#endif

#if !defined(__NR_lookup_dcookie)
#define __NR_lookup_dcookie (__NR_SYSCALL_BASE+249)
#endif

#if !defined(__NR_epoll_create)
#define __NR_epoll_create (__NR_SYSCALL_BASE+250)
#endif

#if !defined(__NR_epoll_ctl)
#define __NR_epoll_ctl (__NR_SYSCALL_BASE+251)
#endif

#if !defined(__NR_epoll_wait)
#define __NR_epoll_wait (__NR_SYSCALL_BASE+252)
#endif

#if !defined(__NR_remap_file_pages)
#define __NR_remap_file_pages (__NR_SYSCALL_BASE+253)
#endif

#if !defined(__NR_set_tid_address)
#define __NR_set_tid_address (__NR_SYSCALL_BASE+256)
#endif

#if !defined(__NR_timer_create)
#define __NR_timer_create (__NR_SYSCALL_BASE+257)
#endif

#if !defined(__NR_timer_settime)
#define __NR_timer_settime (__NR_SYSCALL_BASE+258)
#endif

#if !defined(__NR_timer_gettime)
#define __NR_timer_gettime (__NR_SYSCALL_BASE+259)
#endif

#if !defined(__NR_timer_getoverrun)
#define __NR_timer_getoverrun (__NR_SYSCALL_BASE+260)
#endif

#if !defined(__NR_timer_delete)
#define __NR_timer_delete (__NR_SYSCALL_BASE+261)
#endif

#if !defined(__NR_clock_settime)
#define __NR_clock_settime (__NR_SYSCALL_BASE+262)
#endif

#if !defined(__NR_clock_gettime)
#define __NR_clock_gettime (__NR_SYSCALL_BASE+263)
#endif

#if !defined(__NR_clock_getres)
#define __NR_clock_getres (__NR_SYSCALL_BASE+264)
#endif

#if !defined(__NR_clock_nanosleep)
#define __NR_clock_nanosleep (__NR_SYSCALL_BASE+265)
#endif

#if !defined(__NR_statfs64)
#define __NR_statfs64 (__NR_SYSCALL_BASE+266)
#endif

#if !defined(__NR_fstatfs64)
#define __NR_fstatfs64 (__NR_SYSCALL_BASE+267)
#endif

#if !defined(__NR_tgkill)
#define __NR_tgkill (__NR_SYSCALL_BASE+268)
#endif

#if !defined(__NR_utimes)
#define __NR_utimes (__NR_SYSCALL_BASE+269)
#endif

#if !defined(__NR_arm_fadvise64_64)
#define __NR_arm_fadvise64_64 (__NR_SYSCALL_BASE+270)
#endif

#if !defined(__NR_pciconfig_iobase)
#define __NR_pciconfig_iobase (__NR_SYSCALL_BASE+271)
#endif

#if !defined(__NR_pciconfig_read)
#define __NR_pciconfig_read (__NR_SYSCALL_BASE+272)
#endif

#if !defined(__NR_pciconfig_write)
#define __NR_pciconfig_write (__NR_SYSCALL_BASE+273)
#endif

#if !defined(__NR_mq_open)
#define __NR_mq_open (__NR_SYSCALL_BASE+274)
#endif

#if !defined(__NR_mq_unlink)
#define __NR_mq_unlink (__NR_SYSCALL_BASE+275)
#endif

#if !defined(__NR_mq_timedsend)
#define __NR_mq_timedsend (__NR_SYSCALL_BASE+276)
#endif

#if !defined(__NR_mq_timedreceive)
#define __NR_mq_timedreceive (__NR_SYSCALL_BASE+277)
#endif

#if !defined(__NR_mq_notify)
#define __NR_mq_notify (__NR_SYSCALL_BASE+278)
#endif

#if !defined(__NR_mq_getsetattr)
#define __NR_mq_getsetattr (__NR_SYSCALL_BASE+279)
#endif

#if !defined(__NR_waitid)
#define __NR_waitid (__NR_SYSCALL_BASE+280)
#endif

#if !defined(__NR_socket)
#define __NR_socket (__NR_SYSCALL_BASE+281)
#endif

#if !defined(__NR_bind)
#define __NR_bind (__NR_SYSCALL_BASE+282)
#endif

#if !defined(__NR_connect)
#define __NR_connect (__NR_SYSCALL_BASE+283)
#endif

#if !defined(__NR_listen)
#define __NR_listen (__NR_SYSCALL_BASE+284)
#endif

#if !defined(__NR_accept)
#define __NR_accept (__NR_SYSCALL_BASE+285)
#endif

#if !defined(__NR_getsockname)
#define __NR_getsockname (__NR_SYSCALL_BASE+286)
#endif

#if !defined(__NR_getpeername)
#define __NR_getpeername (__NR_SYSCALL_BASE+287)
#endif

#if !defined(__NR_socketpair)
#define __NR_socketpair (__NR_SYSCALL_BASE+288)
#endif

#if !defined(__NR_send)
#define __NR_send (__NR_SYSCALL_BASE+289)
#endif

#if !defined(__NR_sendto)
#define __NR_sendto (__NR_SYSCALL_BASE+290)
#endif

#if !defined(__NR_recv)
#define __NR_recv (__NR_SYSCALL_BASE+291)
#endif

#if !defined(__NR_recvfrom)
#define __NR_recvfrom (__NR_SYSCALL_BASE+292)
#endif

#if !defined(__NR_shutdown)
#define __NR_shutdown (__NR_SYSCALL_BASE+293)
#endif

#if !defined(__NR_setsockopt)
#define __NR_setsockopt (__NR_SYSCALL_BASE+294)
#endif

#if !defined(__NR_getsockopt)
#define __NR_getsockopt (__NR_SYSCALL_BASE+295)
#endif

#if !defined(__NR_sendmsg)
#define __NR_sendmsg (__NR_SYSCALL_BASE+296)
#endif

#if !defined(__NR_recvmsg)
#define __NR_recvmsg (__NR_SYSCALL_BASE+297)
#endif

#if !defined(__NR_semop)
#define __NR_semop (__NR_SYSCALL_BASE+298)
#endif

#if !defined(__NR_semget)
#define __NR_semget (__NR_SYSCALL_BASE+299)
#endif

#if !defined(__NR_semctl)
#define __NR_semctl (__NR_SYSCALL_BASE+300)
#endif

#if !defined(__NR_msgsnd)
#define __NR_msgsnd (__NR_SYSCALL_BASE+301)
#endif

#if !defined(__NR_msgrcv)
#define __NR_msgrcv (__NR_SYSCALL_BASE+302)
#endif

#if !defined(__NR_msgget)
#define __NR_msgget (__NR_SYSCALL_BASE+303)
#endif

#if !defined(__NR_msgctl)
#define __NR_msgctl (__NR_SYSCALL_BASE+304)
#endif

#if !defined(__NR_shmat)
#define __NR_shmat (__NR_SYSCALL_BASE+305)
#endif

#if !defined(__NR_shmdt)
#define __NR_shmdt (__NR_SYSCALL_BASE+306)
#endif

#if !defined(__NR_shmget)
#define __NR_shmget (__NR_SYSCALL_BASE+307)
#endif

#if !defined(__NR_shmctl)
#define __NR_shmctl (__NR_SYSCALL_BASE+308)
#endif

#if !defined(__NR_add_key)
#define __NR_add_key (__NR_SYSCALL_BASE+309)
#endif

#if !defined(__NR_request_key)
#define __NR_request_key (__NR_SYSCALL_BASE+310)
#endif

#if !defined(__NR_keyctl)
#define __NR_keyctl (__NR_SYSCALL_BASE+311)
#endif

#if !defined(__NR_semtimedop)
#define __NR_semtimedop (__NR_SYSCALL_BASE+312)
#endif

#if !defined(__NR_vserver)
#define __NR_vserver (__NR_SYSCALL_BASE+313)
#endif

#if !defined(__NR_ioprio_set)
#define __NR_ioprio_set (__NR_SYSCALL_BASE+314)
#endif

#if !defined(__NR_ioprio_get)
#define __NR_ioprio_get (__NR_SYSCALL_BASE+315)
#endif

#if !defined(__NR_inotify_init)
#define __NR_inotify_init (__NR_SYSCALL_BASE+316)
#endif

#if !defined(__NR_inotify_add_watch)
#define __NR_inotify_add_watch (__NR_SYSCALL_BASE+317)
#endif

#if !defined(__NR_inotify_rm_watch)
#define __NR_inotify_rm_watch (__NR_SYSCALL_BASE+318)
#endif

#if !defined(__NR_mbind)
#define __NR_mbind (__NR_SYSCALL_BASE+319)
#endif

#if !defined(__NR_get_mempolicy)
#define __NR_get_mempolicy (__NR_SYSCALL_BASE+320)
#endif

#if !defined(__NR_set_mempolicy)
#define __NR_set_mempolicy (__NR_SYSCALL_BASE+321)
#endif

#if !defined(__NR_openat)
#define __NR_openat (__NR_SYSCALL_BASE+322)
#endif

#if !defined(__NR_mkdirat)
#define __NR_mkdirat (__NR_SYSCALL_BASE+323)
#endif

#if !defined(__NR_mknodat)
#define __NR_mknodat (__NR_SYSCALL_BASE+324)
#endif

#if !defined(__NR_fchownat)
#define __NR_fchownat (__NR_SYSCALL_BASE+325)
#endif

#if !defined(__NR_futimesat)
#define __NR_futimesat (__NR_SYSCALL_BASE+326)
#endif

#if !defined(__NR_fstatat64)
#define __NR_fstatat64 (__NR_SYSCALL_BASE+327)
#endif

#if !defined(__NR_unlinkat)
#define __NR_unlinkat (__NR_SYSCALL_BASE+328)
#endif

#if !defined(__NR_renameat)
#define __NR_renameat (__NR_SYSCALL_BASE+329)
#endif

#if !defined(__NR_linkat)
#define __NR_linkat (__NR_SYSCALL_BASE+330)
#endif

#if !defined(__NR_symlinkat)
#define __NR_symlinkat (__NR_SYSCALL_BASE+331)
#endif

#if !defined(__NR_readlinkat)
#define __NR_readlinkat (__NR_SYSCALL_BASE+332)
#endif

#if !defined(__NR_fchmodat)
#define __NR_fchmodat (__NR_SYSCALL_BASE+333)
#endif

#if !defined(__NR_faccessat)
#define __NR_faccessat (__NR_SYSCALL_BASE+334)
#endif

#if !defined(__NR_pselect6)
#define __NR_pselect6 (__NR_SYSCALL_BASE+335)
#endif

#if !defined(__NR_ppoll)
#define __NR_ppoll (__NR_SYSCALL_BASE+336)
#endif

#if !defined(__NR_unshare)
#define __NR_unshare (__NR_SYSCALL_BASE+337)
#endif

#if !defined(__NR_set_robust_list)
#define __NR_set_robust_list (__NR_SYSCALL_BASE+338)
#endif

#if !defined(__NR_get_robust_list)
#define __NR_get_robust_list (__NR_SYSCALL_BASE+339)
#endif

#if !defined(__NR_splice)
#define __NR_splice (__NR_SYSCALL_BASE+340)
#endif

#if !defined(__NR_arm_sync_file_range)
#define __NR_arm_sync_file_range (__NR_SYSCALL_BASE+341)
#endif

#if !defined(__NR_sync_file_range2)
#define __NR_sync_file_range2 (__NR_SYSCALL_BASE+341)
#endif

#if !defined(__NR_tee)
#define __NR_tee (__NR_SYSCALL_BASE+342)
#endif

#if !defined(__NR_vmsplice)
#define __NR_vmsplice (__NR_SYSCALL_BASE+343)
#endif

#if !defined(__NR_move_pages)
#define __NR_move_pages (__NR_SYSCALL_BASE+344)
#endif

#if !defined(__NR_getcpu)
#define __NR_getcpu (__NR_SYSCALL_BASE+345)
#endif

#if !defined(__NR_epoll_pwait)
#define __NR_epoll_pwait (__NR_SYSCALL_BASE+346)
#endif

#if !defined(__NR_kexec_load)
#define __NR_kexec_load (__NR_SYSCALL_BASE+347)
#endif

#if !defined(__NR_utimensat)
#define __NR_utimensat (__NR_SYSCALL_BASE+348)
#endif

#if !defined(__NR_signalfd)
#define __NR_signalfd (__NR_SYSCALL_BASE+349)
#endif

#if !defined(__NR_timerfd_create)
#define __NR_timerfd_create (__NR_SYSCALL_BASE+350)
#endif

#if !defined(__NR_eventfd)
#define __NR_eventfd (__NR_SYSCALL_BASE+351)
#endif

#if !defined(__NR_fallocate)
#define __NR_fallocate (__NR_SYSCALL_BASE+352)
#endif

#if !defined(__NR_timerfd_settime)
#define __NR_timerfd_settime (__NR_SYSCALL_BASE+353)
#endif

#if !defined(__NR_timerfd_gettime)
#define __NR_timerfd_gettime (__NR_SYSCALL_BASE+354)
#endif

#if !defined(__NR_signalfd4)
#define __NR_signalfd4 (__NR_SYSCALL_BASE+355)
#endif

#if !defined(__NR_eventfd2)
#define __NR_eventfd2 (__NR_SYSCALL_BASE+356)
#endif

#if !defined(__NR_epoll_create1)
#define __NR_epoll_create1 (__NR_SYSCALL_BASE+357)
#endif

#if !defined(__NR_dup3)
#define __NR_dup3 (__NR_SYSCALL_BASE+358)
#endif

#if !defined(__NR_pipe2)
#define __NR_pipe2 (__NR_SYSCALL_BASE+359)
#endif

#if !defined(__NR_inotify_init1)
#define __NR_inotify_init1 (__NR_SYSCALL_BASE+360)
#endif

#if !defined(__NR_preadv)
#define __NR_preadv (__NR_SYSCALL_BASE+361)
#endif

#if !defined(__NR_pwritev)
#define __NR_pwritev (__NR_SYSCALL_BASE+362)
#endif

#if !defined(__NR_rt_tgsigqueueinfo)
#define __NR_rt_tgsigqueueinfo (__NR_SYSCALL_BASE+363)
#endif

#if !defined(__NR_perf_event_open)
#define __NR_perf_event_open (__NR_SYSCALL_BASE+364)
#endif

#if !defined(__NR_recvmmsg)
#define __NR_recvmmsg (__NR_SYSCALL_BASE+365)
#endif

#if !defined(__NR_accept4)
#define __NR_accept4 (__NR_SYSCALL_BASE+366)
#endif

#if !defined(__NR_fanotify_init)
#define __NR_fanotify_init (__NR_SYSCALL_BASE+367)
#endif

#if !defined(__NR_fanotify_mark)
#define __NR_fanotify_mark (__NR_SYSCALL_BASE+368)
#endif

#if !defined(__NR_prlimit64)
#define __NR_prlimit64 (__NR_SYSCALL_BASE+369)
#endif

#if !defined(__NR_name_to_handle_at)
#define __NR_name_to_handle_at (__NR_SYSCALL_BASE+370)
#endif

#if !defined(__NR_open_by_handle_at)
#define __NR_open_by_handle_at (__NR_SYSCALL_BASE+371)
#endif

#if !defined(__NR_clock_adjtime)
#define __NR_clock_adjtime (__NR_SYSCALL_BASE+372)
#endif

#if !defined(__NR_syncfs)
#define __NR_syncfs (__NR_SYSCALL_BASE+373)
#endif

#if !defined(__NR_sendmmsg)
#define __NR_sendmmsg (__NR_SYSCALL_BASE+374)
#endif

#if !defined(__NR_setns)
#define __NR_setns (__NR_SYSCALL_BASE+375)
#endif

#if !defined(__NR_process_vm_readv)
#define __NR_process_vm_readv (__NR_SYSCALL_BASE+376)
#endif

#if !defined(__NR_process_vm_writev)
#define __NR_process_vm_writev (__NR_SYSCALL_BASE+377)
#endif

#if !defined(__NR_kcmp)
#define __NR_kcmp (__NR_SYSCALL_BASE+378)
#endif

#if !defined(__NR_finit_module)
#define __NR_finit_module (__NR_SYSCALL_BASE+379)
#endif

#if !defined(__NR_sched_setattr)
#define __NR_sched_setattr (__NR_SYSCALL_BASE+380)
#endif

#if !defined(__NR_sched_getattr)
#define __NR_sched_getattr (__NR_SYSCALL_BASE+381)
#endif

#if !defined(__NR_renameat2)
#define __NR_renameat2 (__NR_SYSCALL_BASE+382)
#endif

#if !defined(__NR_seccomp)
#define __NR_seccomp (__NR_SYSCALL_BASE+383)
#endif

#if !defined(__NR_getrandom)
#define __NR_getrandom (__NR_SYSCALL_BASE+384)
#endif

#if !defined(__NR_memfd_create)
#define __NR_memfd_create (__NR_SYSCALL_BASE+385)
#endif

#if !defined(__NR_bpf)
#define __NR_bpf (__NR_SYSCALL_BASE+386)
#endif

#if !defined(__NR_execveat)
#define __NR_execveat (__NR_SYSCALL_BASE+387)
#endif

#if !defined(__NR_userfaultfd)
#define __NR_userfaultfd (__NR_SYSCALL_BASE+388)
#endif

#if !defined(__NR_membarrier)
#define __NR_membarrier (__NR_SYSCALL_BASE+389)
#endif

#if !defined(__NR_mlock2)
#define __NR_mlock2 (__NR_SYSCALL_BASE+390)
#endif

#if !defined(__NR_copy_file_range)
#define __NR_copy_file_range (__NR_SYSCALL_BASE+391)
#endif

#if !defined(__NR_preadv2)
#define __NR_preadv2 (__NR_SYSCALL_BASE+392)
#endif

#if !defined(__NR_pwritev2)
#define __NR_pwritev2 (__NR_SYSCALL_BASE+393)
#endif

#if !defined(__NR_pkey_mprotect)
#define __NR_pkey_mprotect (__NR_SYSCALL_BASE+394)
#endif

#if !defined(__NR_pkey_alloc)
#define __NR_pkey_alloc (__NR_SYSCALL_BASE+395)
#endif

#if !defined(__NR_pkey_free)
#define __NR_pkey_free (__NR_SYSCALL_BASE+396)
#endif

#if !defined(__NR_statx)
#define __NR_statx (__NR_SYSCALL_BASE+397)
#endif

#if !defined(__NR_rseq)
#define __NR_rseq (__NR_SYSCALL_BASE+398)
#endif

#if !defined(__NR_io_pgetevents)
#define __NR_io_pgetevents (__NR_SYSCALL_BASE+399)
#endif

#if !defined(__NR_migrate_pages)
#define __NR_migrate_pages (__NR_SYSCALL_BASE + 400)
#endif

#if !defined(__NR_kexec_file_load)
#define __NR_kexec_file_load (__NR_SYSCALL_BASE + 401)
#endif

#if !defined(__NR_clock_gettime64)
#define __NR_clock_gettime64 (__NR_SYSCALL_BASE + 403)
#endif

#if !defined(__NR_clock_settime64)
#define __NR_clock_settime64 (__NR_SYSCALL_BASE + 404)
#endif

#if !defined(__NR_clock_adjtime64)
#define __NR_clock_adjtime64 (__NR_SYSCALL_BASE + 405)
#endif

#if !defined(__NR_clock_getres_time64)
#define __NR_clock_getres_time64 (__NR_SYSCALL_BASE + 406)
#endif

#if !defined(__NR_clock_nanosleep_time64)
#define __NR_clock_nanosleep_time64 (__NR_SYSCALL_BASE + 407)
#endif

#if !defined(__NR_timer_gettime64)
#define __NR_timer_gettime64 (__NR_SYSCALL_BASE + 408)
#endif

#if !defined(__NR_timer_settime64)
#define __NR_timer_settime64 (__NR_SYSCALL_BASE + 409)
#endif

#if !defined(__NR_timerfd_gettime64)
#define __NR_timerfd_gettime64 (__NR_SYSCALL_BASE + 410)
#endif

#if !defined(__NR_timerfd_settime64)
#define __NR_timerfd_settime64 (__NR_SYSCALL_BASE + 411)
#endif

#if !defined(__NR_utimensat_time64)
#define __NR_utimensat_time64 (__NR_SYSCALL_BASE + 412)
#endif

#if !defined(__NR_pselect6_time64)
#define __NR_pselect6_time64 (__NR_SYSCALL_BASE + 413)
#endif

#if !defined(__NR_ppoll_time64)
#define __NR_ppoll_time64 (__NR_SYSCALL_BASE + 414)
#endif

#if !defined(__NR_io_pgetevents_time64)
#define __NR_io_pgetevents_time64 (__NR_SYSCALL_BASE + 416)
#endif

#if !defined(__NR_recvmmsg_time64)
#define __NR_recvmmsg_time64 (__NR_SYSCALL_BASE + 417)
#endif

#if !defined(__NR_mq_timedsend_time64)
#define __NR_mq_timedsend_time64 (__NR_SYSCALL_BASE + 418)
#endif

#if !defined(__NR_mq_timedreceive_time64)
#define __NR_mq_timedreceive_time64 (__NR_SYSCALL_BASE + 419)
#endif

#if !defined(__NR_semtimedop_time64)
#define __NR_semtimedop_time64 (__NR_SYSCALL_BASE + 420)
#endif

#if !defined(__NR_rt_sigtimedwait_time64)
#define __NR_rt_sigtimedwait_time64 (__NR_SYSCALL_BASE + 421)
#endif

#if !defined(__NR_futex_time64)
#define __NR_futex_time64 (__NR_SYSCALL_BASE + 422)
#endif

#if !defined(__NR_sched_rr_get_interval_time64)
#define __NR_sched_rr_get_interval_time64 (__NR_SYSCALL_BASE + 423)
#endif

#if !defined(__NR_pidfd_send_signal)
#define __NR_pidfd_send_signal (__NR_SYSCALL_BASE + 424)
#endif

#if !defined(__NR_io_uring_setup)
#define __NR_io_uring_setup (__NR_SYSCALL_BASE + 425)
#endif

#if !defined(__NR_io_uring_enter)
#define __NR_io_uring_enter (__NR_SYSCALL_BASE + 426)
#endif

#if !defined(__NR_io_uring_register)
#define __NR_io_uring_register (__NR_SYSCALL_BASE + 427)
#endif

#if !defined(__NR_open_tree)
#define __NR_open_tree (__NR_SYSCALL_BASE + 428)
#endif

#if !defined(__NR_move_mount)
#define __NR_move_mount (__NR_SYSCALL_BASE + 429)
#endif

#if !defined(__NR_fsopen)
#define __NR_fsopen (__NR_SYSCALL_BASE + 430)
#endif

#if !defined(__NR_fsconfig)
#define __NR_fsconfig (__NR_SYSCALL_BASE + 431)
#endif

#if !defined(__NR_fsmount)
#define __NR_fsmount (__NR_SYSCALL_BASE + 432)
#endif

#if !defined(__NR_fspick)
#define __NR_fspick (__NR_SYSCALL_BASE + 433)
#endif

#if !defined(__NR_pidfd_open)
#define __NR_pidfd_open (__NR_SYSCALL_BASE + 434)
#endif

#if !defined(__NR_clone3)
#define __NR_clone3 (__NR_SYSCALL_BASE + 435)
#endif

#if !defined(__NR_close_range)
#define __NR_close_range (__NR_SYSCALL_BASE + 436)
#endif

#if !defined(__NR_openat2)
#define __NR_openat2 (__NR_SYSCALL_BASE + 437)
#endif

#if !defined(__NR_pidfd_getfd)
#define __NR_pidfd_getfd (__NR_SYSCALL_BASE + 438)
#endif

#if !defined(__NR_faccessat2)
#define __NR_faccessat2 (__NR_SYSCALL_BASE + 439)
#endif

#if !defined(__NR_process_madvise)
#define __NR_process_madvise (__NR_SYSCALL_BASE + 440)
#endif

#if !defined(__NR_epoll_pwait2)
#define __NR_epoll_pwait2 (__NR_SYSCALL_BASE + 441)
#endif

#if !defined(__NR_mount_setattr)
#define __NR_mount_setattr (__NR_SYSCALL_BASE + 442)
#endif

#if !defined(__NR_landlock_create_ruleset)
#define __NR_landlock_create_ruleset (__NR_SYSCALL_BASE + 444)
#endif

#if !defined(__NR_landlock_add_rule)
#define __NR_landlock_add_rule (__NR_SYSCALL_BASE + 445)
#endif

#if !defined(__NR_landlock_restrict_self)
#define __NR_landlock_restrict_self (__NR_SYSCALL_BASE + 446)
#endif

#if !defined(__NR_process_mrelease)
#define __NR_process_mrelease (__NR_SYSCALL_BASE + 448)
#endif

#if !defined(__NR_futex_waitv)
#define __NR_futex_waitv (__NR_SYSCALL_BASE + 449)
#endif

#if !defined(__NR_set_mempolicy_home_node)
#define __NR_set_mempolicy_home_node (__NR_SYSCALL_BASE + 450)
#endif

#if !defined(__NR_cachestat)
#define __NR_cachestat (__NR_SYSCALL_BASE + 451)
#endif

#if !defined(__NR_fchmodat2)
#define __NR_fchmodat2 (__NR_SYSCALL_BASE + 452)
#endif

#if !defined(__NR_map_shadow_stack)
#define __NR_map_shadow_stack (__NR_SYSCALL_BASE + 453)
#endif

#if !defined(__NR_futex_wake)
#define __NR_futex_wake (__NR_SYSCALL_BASE + 454)
#endif

#if !defined(__NR_futex_wait)
#define __NR_futex_wait (__NR_SYSCALL_BASE + 455)
#endif

#if !defined(__NR_futex_requeue)
#define __NR_futex_requeue (__NR_SYSCALL_BASE + 456)
#endif

#if !defined(__NR_statmount)
#define __NR_statmount (__NR_SYSCALL_BASE + 457)
#endif

#if !defined(__NR_listmount)
#define __NR_listmount (__NR_SYSCALL_BASE + 458)
#endif

#if !defined(__NR_lsm_get_self_attr)
#define __NR_lsm_get_self_attr (__NR_SYSCALL_BASE + 459)
#endif

#if !defined(__NR_lsm_set_self_attr)
#define __NR_lsm_set_self_attr (__NR_SYSCALL_BASE + 460)
#endif

#if !defined(__NR_lsm_list_modules)
#define __NR_lsm_list_modules (__NR_SYSCALL_BASE + 461)
#endif

#if !defined(__NR_mseal)
#define __NR_mseal (__NR_SYSCALL_BASE + 462)
#endif

// ARM private syscalls.
#if !defined(__ARM_NR_BASE)
#define __ARM_NR_BASE (__NR_SYSCALL_BASE + 0xF0000)
#endif

#if !defined(__ARM_NR_breakpoint)
#define __ARM_NR_breakpoint (__ARM_NR_BASE+1)
#endif

#if !defined(__ARM_NR_cacheflush)
#define __ARM_NR_cacheflush (__ARM_NR_BASE+2)
#endif

#if !defined(__ARM_NR_usr26)
#define __ARM_NR_usr26 (__ARM_NR_BASE+3)
#endif

#if !defined(__ARM_NR_usr32)
#define __ARM_NR_usr32 (__ARM_NR_BASE+4)
#endif

#if !defined(__ARM_NR_set_tls)
#define __ARM_NR_set_tls (__ARM_NR_BASE+5)
#endif

// ARM kernel private syscall.
#if !defined(__ARM_NR_cmpxchg)
#define __ARM_NR_cmpxchg (__ARM_NR_BASE+0x00fff0)
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_ARM_LINUX_SYSCALLS_H_
