// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generated from the Linux kernel's calls.S.
#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_MIPS_LINUX_SYSCALLS_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_MIPS_LINUX_SYSCALLS_H_

#if !defined(__mips__)
#error "Including header on wrong architecture"
#endif

// __NR_Linux, is defined in <asm/unistd.h>.
#include <asm/unistd.h>

#if !defined(__NR_syscall)
#define __NR_syscall (__NR_Linux + 0)
#endif

#if !defined(__NR_exit)
#define __NR_exit (__NR_Linux + 1)
#endif

#if !defined(__NR_fork)
#define __NR_fork (__NR_Linux + 2)
#endif

#if !defined(__NR_read)
#define __NR_read (__NR_Linux + 3)
#endif

#if !defined(__NR_write)
#define __NR_write (__NR_Linux + 4)
#endif

#if !defined(__NR_open)
#define __NR_open (__NR_Linux + 5)
#endif

#if !defined(__NR_close)
#define __NR_close (__NR_Linux + 6)
#endif

#if !defined(__NR_waitpid)
#define __NR_waitpid (__NR_Linux + 7)
#endif

#if !defined(__NR_creat)
#define __NR_creat (__NR_Linux + 8)
#endif

#if !defined(__NR_link)
#define __NR_link (__NR_Linux + 9)
#endif

#if !defined(__NR_unlink)
#define __NR_unlink (__NR_Linux + 10)
#endif

#if !defined(__NR_execve)
#define __NR_execve (__NR_Linux + 11)
#endif

#if !defined(__NR_chdir)
#define __NR_chdir (__NR_Linux + 12)
#endif

#if !defined(__NR_time)
#define __NR_time (__NR_Linux + 13)
#endif

#if !defined(__NR_mknod)
#define __NR_mknod (__NR_Linux + 14)
#endif

#if !defined(__NR_chmod)
#define __NR_chmod (__NR_Linux + 15)
#endif

#if !defined(__NR_lchown)
#define __NR_lchown (__NR_Linux + 16)
#endif

#if !defined(__NR_break)
#define __NR_break (__NR_Linux + 17)
#endif

#if !defined(__NR_unused18)
#define __NR_unused18 (__NR_Linux + 18)
#endif

#if !defined(__NR_lseek)
#define __NR_lseek (__NR_Linux + 19)
#endif

#if !defined(__NR_getpid)
#define __NR_getpid (__NR_Linux + 20)
#endif

#if !defined(__NR_mount)
#define __NR_mount (__NR_Linux + 21)
#endif

#if !defined(__NR_umount)
#define __NR_umount (__NR_Linux + 22)
#endif

#if !defined(__NR_setuid)
#define __NR_setuid (__NR_Linux + 23)
#endif

#if !defined(__NR_getuid)
#define __NR_getuid (__NR_Linux + 24)
#endif

#if !defined(__NR_stime)
#define __NR_stime (__NR_Linux + 25)
#endif

#if !defined(__NR_ptrace)
#define __NR_ptrace (__NR_Linux + 26)
#endif

#if !defined(__NR_alarm)
#define __NR_alarm (__NR_Linux + 27)
#endif

#if !defined(__NR_unused28)
#define __NR_unused28 (__NR_Linux + 28)
#endif

#if !defined(__NR_pause)
#define __NR_pause (__NR_Linux + 29)
#endif

#if !defined(__NR_utime)
#define __NR_utime (__NR_Linux + 30)
#endif

#if !defined(__NR_stty)
#define __NR_stty (__NR_Linux + 31)
#endif

#if !defined(__NR_gtty)
#define __NR_gtty (__NR_Linux + 32)
#endif

#if !defined(__NR_access)
#define __NR_access (__NR_Linux + 33)
#endif

#if !defined(__NR_nice)
#define __NR_nice (__NR_Linux + 34)
#endif

#if !defined(__NR_ftime)
#define __NR_ftime (__NR_Linux + 35)
#endif

#if !defined(__NR_sync)
#define __NR_sync (__NR_Linux + 36)
#endif

#if !defined(__NR_kill)
#define __NR_kill (__NR_Linux + 37)
#endif

#if !defined(__NR_rename)
#define __NR_rename (__NR_Linux + 38)
#endif

#if !defined(__NR_mkdir)
#define __NR_mkdir (__NR_Linux + 39)
#endif

#if !defined(__NR_rmdir)
#define __NR_rmdir (__NR_Linux + 40)
#endif

#if !defined(__NR_dup)
#define __NR_dup (__NR_Linux + 41)
#endif

#if !defined(__NR_pipe)
#define __NR_pipe (__NR_Linux + 42)
#endif

#if !defined(__NR_times)
#define __NR_times (__NR_Linux + 43)
#endif

#if !defined(__NR_prof)
#define __NR_prof (__NR_Linux + 44)
#endif

#if !defined(__NR_brk)
#define __NR_brk (__NR_Linux + 45)
#endif

#if !defined(__NR_setgid)
#define __NR_setgid (__NR_Linux + 46)
#endif

#if !defined(__NR_getgid)
#define __NR_getgid (__NR_Linux + 47)
#endif

#if !defined(__NR_signal)
#define __NR_signal (__NR_Linux + 48)
#endif

#if !defined(__NR_geteuid)
#define __NR_geteuid (__NR_Linux + 49)
#endif

#if !defined(__NR_getegid)
#define __NR_getegid (__NR_Linux + 50)
#endif

#if !defined(__NR_acct)
#define __NR_acct (__NR_Linux + 51)
#endif

#if !defined(__NR_umount2)
#define __NR_umount2 (__NR_Linux + 52)
#endif

#if !defined(__NR_lock)
#define __NR_lock (__NR_Linux + 53)
#endif

#if !defined(__NR_ioctl)
#define __NR_ioctl (__NR_Linux + 54)
#endif

#if !defined(__NR_fcntl)
#define __NR_fcntl (__NR_Linux + 55)
#endif

#if !defined(__NR_mpx)
#define __NR_mpx (__NR_Linux + 56)
#endif

#if !defined(__NR_setpgid)
#define __NR_setpgid (__NR_Linux + 57)
#endif

#if !defined(__NR_ulimit)
#define __NR_ulimit (__NR_Linux + 58)
#endif

#if !defined(__NR_unused59)
#define __NR_unused59 (__NR_Linux + 59)
#endif

#if !defined(__NR_umask)
#define __NR_umask (__NR_Linux + 60)
#endif

#if !defined(__NR_chroot)
#define __NR_chroot (__NR_Linux + 61)
#endif

#if !defined(__NR_ustat)
#define __NR_ustat (__NR_Linux + 62)
#endif

#if !defined(__NR_dup2)
#define __NR_dup2 (__NR_Linux + 63)
#endif

#if !defined(__NR_getppid)
#define __NR_getppid (__NR_Linux + 64)
#endif

#if !defined(__NR_getpgrp)
#define __NR_getpgrp (__NR_Linux + 65)
#endif

#if !defined(__NR_setsid)
#define __NR_setsid (__NR_Linux + 66)
#endif

#if !defined(__NR_sigaction)
#define __NR_sigaction (__NR_Linux + 67)
#endif

#if !defined(__NR_sgetmask)
#define __NR_sgetmask (__NR_Linux + 68)
#endif

#if !defined(__NR_ssetmask)
#define __NR_ssetmask (__NR_Linux + 69)
#endif

#if !defined(__NR_setreuid)
#define __NR_setreuid (__NR_Linux + 70)
#endif

#if !defined(__NR_setregid)
#define __NR_setregid (__NR_Linux + 71)
#endif

#if !defined(__NR_sigsuspend)
#define __NR_sigsuspend (__NR_Linux + 72)
#endif

#if !defined(__NR_sigpending)
#define __NR_sigpending (__NR_Linux + 73)
#endif

#if !defined(__NR_sethostname)
#define __NR_sethostname (__NR_Linux + 74)
#endif

#if !defined(__NR_setrlimit)
#define __NR_setrlimit (__NR_Linux + 75)
#endif

#if !defined(__NR_getrlimit)
#define __NR_getrlimit (__NR_Linux + 76)
#endif

#if !defined(__NR_getrusage)
#define __NR_getrusage (__NR_Linux + 77)
#endif

#if !defined(__NR_gettimeofday)
#define __NR_gettimeofday (__NR_Linux + 78)
#endif

#if !defined(__NR_settimeofday)
#define __NR_settimeofday (__NR_Linux + 79)
#endif

#if !defined(__NR_getgroups)
#define __NR_getgroups (__NR_Linux + 80)
#endif

#if !defined(__NR_setgroups)
#define __NR_setgroups (__NR_Linux + 81)
#endif

#if !defined(__NR_reserved82)
#define __NR_reserved82 (__NR_Linux + 82)
#endif

#if !defined(__NR_symlink)
#define __NR_symlink (__NR_Linux + 83)
#endif

#if !defined(__NR_unused84)
#define __NR_unused84 (__NR_Linux + 84)
#endif

#if !defined(__NR_readlink)
#define __NR_readlink (__NR_Linux + 85)
#endif

#if !defined(__NR_uselib)
#define __NR_uselib (__NR_Linux + 86)
#endif

#if !defined(__NR_swapon)
#define __NR_swapon (__NR_Linux + 87)
#endif

#if !defined(__NR_reboot)
#define __NR_reboot (__NR_Linux + 88)
#endif

#if !defined(__NR_readdir)
#define __NR_readdir (__NR_Linux + 89)
#endif

#if !defined(__NR_mmap)
#define __NR_mmap (__NR_Linux + 90)
#endif

#if !defined(__NR_munmap)
#define __NR_munmap (__NR_Linux + 91)
#endif

#if !defined(__NR_truncate)
#define __NR_truncate (__NR_Linux + 92)
#endif

#if !defined(__NR_ftruncate)
#define __NR_ftruncate (__NR_Linux + 93)
#endif

#if !defined(__NR_fchmod)
#define __NR_fchmod (__NR_Linux + 94)
#endif

#if !defined(__NR_fchown)
#define __NR_fchown (__NR_Linux + 95)
#endif

#if !defined(__NR_getpriority)
#define __NR_getpriority (__NR_Linux + 96)
#endif

#if !defined(__NR_setpriority)
#define __NR_setpriority (__NR_Linux + 97)
#endif

#if !defined(__NR_profil)
#define __NR_profil (__NR_Linux + 98)
#endif

#if !defined(__NR_statfs)
#define __NR_statfs (__NR_Linux + 99)
#endif

#if !defined(__NR_fstatfs)
#define __NR_fstatfs (__NR_Linux + 100)
#endif

#if !defined(__NR_ioperm)
#define __NR_ioperm (__NR_Linux + 101)
#endif

#if !defined(__NR_socketcall)
#define __NR_socketcall (__NR_Linux + 102)
#endif

#if !defined(__NR_syslog)
#define __NR_syslog (__NR_Linux + 103)
#endif

#if !defined(__NR_setitimer)
#define __NR_setitimer (__NR_Linux + 104)
#endif

#if !defined(__NR_getitimer)
#define __NR_getitimer (__NR_Linux + 105)
#endif

#if !defined(__NR_stat)
#define __NR_stat (__NR_Linux + 106)
#endif

#if !defined(__NR_lstat)
#define __NR_lstat (__NR_Linux + 107)
#endif

#if !defined(__NR_fstat)
#define __NR_fstat (__NR_Linux + 108)
#endif

#if !defined(__NR_unused109)
#define __NR_unused109 (__NR_Linux + 109)
#endif

#if !defined(__NR_iopl)
#define __NR_iopl (__NR_Linux + 110)
#endif

#if !defined(__NR_vhangup)
#define __NR_vhangup (__NR_Linux + 111)
#endif

#if !defined(__NR_idle)
#define __NR_idle (__NR_Linux + 112)
#endif

#if !defined(__NR_vm86)
#define __NR_vm86 (__NR_Linux + 113)
#endif

#if !defined(__NR_wait4)
#define __NR_wait4 (__NR_Linux + 114)
#endif

#if !defined(__NR_swapoff)
#define __NR_swapoff (__NR_Linux + 115)
#endif

#if !defined(__NR_sysinfo)
#define __NR_sysinfo (__NR_Linux + 116)
#endif

#if !defined(__NR_ipc)
#define __NR_ipc (__NR_Linux + 117)
#endif

#if !defined(__NR_fsync)
#define __NR_fsync (__NR_Linux + 118)
#endif

#if !defined(__NR_sigreturn)
#define __NR_sigreturn (__NR_Linux + 119)
#endif

#if !defined(__NR_clone)
#define __NR_clone (__NR_Linux + 120)
#endif

#if !defined(__NR_setdomainname)
#define __NR_setdomainname (__NR_Linux + 121)
#endif

#if !defined(__NR_uname)
#define __NR_uname (__NR_Linux + 122)
#endif

#if !defined(__NR_modify_ldt)
#define __NR_modify_ldt (__NR_Linux + 123)
#endif

#if !defined(__NR_adjtimex)
#define __NR_adjtimex (__NR_Linux + 124)
#endif

#if !defined(__NR_mprotect)
#define __NR_mprotect (__NR_Linux + 125)
#endif

#if !defined(__NR_sigprocmask)
#define __NR_sigprocmask (__NR_Linux + 126)
#endif

#if !defined(__NR_create_module)
#define __NR_create_module (__NR_Linux + 127)
#endif

#if !defined(__NR_init_module)
#define __NR_init_module (__NR_Linux + 128)
#endif

#if !defined(__NR_delete_module)
#define __NR_delete_module (__NR_Linux + 129)
#endif

#if !defined(__NR_get_kernel_syms)
#define __NR_get_kernel_syms (__NR_Linux + 130)
#endif

#if !defined(__NR_quotactl)
#define __NR_quotactl (__NR_Linux + 131)
#endif

#if !defined(__NR_getpgid)
#define __NR_getpgid (__NR_Linux + 132)
#endif

#if !defined(__NR_fchdir)
#define __NR_fchdir (__NR_Linux + 133)
#endif

#if !defined(__NR_bdflush)
#define __NR_bdflush (__NR_Linux + 134)
#endif

#if !defined(__NR_sysfs)
#define __NR_sysfs (__NR_Linux + 135)
#endif

#if !defined(__NR_personality)
#define __NR_personality (__NR_Linux + 136)
#endif

#if !defined(__NR_afs_syscall)
#define __NR_afs_syscall                               \
  (__NR_Linux + 137) /* Syscall for Andrew File System \
                        */
#endif

#if !defined(__NR_setfsuid)
#define __NR_setfsuid (__NR_Linux + 138)
#endif

#if !defined(__NR_setfsgid)
#define __NR_setfsgid (__NR_Linux + 139)
#endif

#if !defined(__NR__llseek)
#define __NR__llseek (__NR_Linux + 140)
#endif

#if !defined(__NR_getdents)
#define __NR_getdents (__NR_Linux + 141)
#endif

#if !defined(__NR__newselect)
#define __NR__newselect (__NR_Linux + 142)
#endif

#if !defined(__NR_flock)
#define __NR_flock (__NR_Linux + 143)
#endif

#if !defined(__NR_msync)
#define __NR_msync (__NR_Linux + 144)
#endif

#if !defined(__NR_readv)
#define __NR_readv (__NR_Linux + 145)
#endif

#if !defined(__NR_writev)
#define __NR_writev (__NR_Linux + 146)
#endif

#if !defined(__NR_cacheflush)
#define __NR_cacheflush (__NR_Linux + 147)
#endif

#if !defined(__NR_cachectl)
#define __NR_cachectl (__NR_Linux + 148)
#endif

#if !defined(__NR_sysmips)
#define __NR_sysmips (__NR_Linux + 149)
#endif

#if !defined(__NR_unused150)
#define __NR_unused150 (__NR_Linux + 150)
#endif

#if !defined(__NR_getsid)
#define __NR_getsid (__NR_Linux + 151)
#endif

#if !defined(__NR_fdatasync)
#define __NR_fdatasync (__NR_Linux + 152)
#endif

#if !defined(__NR__sysctl)
#define __NR__sysctl (__NR_Linux + 153)
#endif

#if !defined(__NR_mlock)
#define __NR_mlock (__NR_Linux + 154)
#endif

#if !defined(__NR_munlock)
#define __NR_munlock (__NR_Linux + 155)
#endif

#if !defined(__NR_mlockall)
#define __NR_mlockall (__NR_Linux + 156)
#endif

#if !defined(__NR_munlockall)
#define __NR_munlockall (__NR_Linux + 157)
#endif

#if !defined(__NR_sched_setparam)
#define __NR_sched_setparam (__NR_Linux + 158)
#endif

#if !defined(__NR_sched_getparam)
#define __NR_sched_getparam (__NR_Linux + 159)
#endif

#if !defined(__NR_sched_setscheduler)
#define __NR_sched_setscheduler (__NR_Linux + 160)
#endif

#if !defined(__NR_sched_getscheduler)
#define __NR_sched_getscheduler (__NR_Linux + 161)
#endif

#if !defined(__NR_sched_yield)
#define __NR_sched_yield (__NR_Linux + 162)
#endif

#if !defined(__NR_sched_get_priority_max)
#define __NR_sched_get_priority_max (__NR_Linux + 163)
#endif

#if !defined(__NR_sched_get_priority_min)
#define __NR_sched_get_priority_min (__NR_Linux + 164)
#endif

#if !defined(__NR_sched_rr_get_interval)
#define __NR_sched_rr_get_interval (__NR_Linux + 165)
#endif

#if !defined(__NR_nanosleep)
#define __NR_nanosleep (__NR_Linux + 166)
#endif

#if !defined(__NR_mremap)
#define __NR_mremap (__NR_Linux + 167)
#endif

#if !defined(__NR_accept)
#define __NR_accept (__NR_Linux + 168)
#endif

#if !defined(__NR_bind)
#define __NR_bind (__NR_Linux + 169)
#endif

#if !defined(__NR_connect)
#define __NR_connect (__NR_Linux + 170)
#endif

#if !defined(__NR_getpeername)
#define __NR_getpeername (__NR_Linux + 171)
#endif

#if !defined(__NR_getsockname)
#define __NR_getsockname (__NR_Linux + 172)
#endif

#if !defined(__NR_getsockopt)
#define __NR_getsockopt (__NR_Linux + 173)
#endif

#if !defined(__NR_listen)
#define __NR_listen (__NR_Linux + 174)
#endif

#if !defined(__NR_recv)
#define __NR_recv (__NR_Linux + 175)
#endif

#if !defined(__NR_recvfrom)
#define __NR_recvfrom (__NR_Linux + 176)
#endif

#if !defined(__NR_recvmsg)
#define __NR_recvmsg (__NR_Linux + 177)
#endif

#if !defined(__NR_send)
#define __NR_send (__NR_Linux + 178)
#endif

#if !defined(__NR_sendmsg)
#define __NR_sendmsg (__NR_Linux + 179)
#endif

#if !defined(__NR_sendto)
#define __NR_sendto (__NR_Linux + 180)
#endif

#if !defined(__NR_setsockopt)
#define __NR_setsockopt (__NR_Linux + 181)
#endif

#if !defined(__NR_shutdown)
#define __NR_shutdown (__NR_Linux + 182)
#endif

#if !defined(__NR_socket)
#define __NR_socket (__NR_Linux + 183)
#endif

#if !defined(__NR_socketpair)
#define __NR_socketpair (__NR_Linux + 184)
#endif

#if !defined(__NR_setresuid)
#define __NR_setresuid (__NR_Linux + 185)
#endif

#if !defined(__NR_getresuid)
#define __NR_getresuid (__NR_Linux + 186)
#endif

#if !defined(__NR_query_module)
#define __NR_query_module (__NR_Linux + 187)
#endif

#if !defined(__NR_poll)
#define __NR_poll (__NR_Linux + 188)
#endif

#if !defined(__NR_nfsservctl)
#define __NR_nfsservctl (__NR_Linux + 189)
#endif

#if !defined(__NR_setresgid)
#define __NR_setresgid (__NR_Linux + 190)
#endif

#if !defined(__NR_getresgid)
#define __NR_getresgid (__NR_Linux + 191)
#endif

#if !defined(__NR_prctl)
#define __NR_prctl (__NR_Linux + 192)
#endif

#if !defined(__NR_rt_sigreturn)
#define __NR_rt_sigreturn (__NR_Linux + 193)
#endif

#if !defined(__NR_rt_sigaction)
#define __NR_rt_sigaction (__NR_Linux + 194)
#endif

#if !defined(__NR_rt_sigprocmask)
#define __NR_rt_sigprocmask (__NR_Linux + 195)
#endif

#if !defined(__NR_rt_sigpending)
#define __NR_rt_sigpending (__NR_Linux + 196)
#endif

#if !defined(__NR_rt_sigtimedwait)
#define __NR_rt_sigtimedwait (__NR_Linux + 197)
#endif

#if !defined(__NR_rt_sigqueueinfo)
#define __NR_rt_sigqueueinfo (__NR_Linux + 198)
#endif

#if !defined(__NR_rt_sigsuspend)
#define __NR_rt_sigsuspend (__NR_Linux + 199)
#endif

#if !defined(__NR_pread64)
#define __NR_pread64 (__NR_Linux + 200)
#endif

#if !defined(__NR_pwrite64)
#define __NR_pwrite64 (__NR_Linux + 201)
#endif

#if !defined(__NR_chown)
#define __NR_chown (__NR_Linux + 202)
#endif

#if !defined(__NR_getcwd)
#define __NR_getcwd (__NR_Linux + 203)
#endif

#if !defined(__NR_capget)
#define __NR_capget (__NR_Linux + 204)
#endif

#if !defined(__NR_capset)
#define __NR_capset (__NR_Linux + 205)
#endif

#if !defined(__NR_sigaltstack)
#define __NR_sigaltstack (__NR_Linux + 206)
#endif

#if !defined(__NR_sendfile)
#define __NR_sendfile (__NR_Linux + 207)
#endif

#if !defined(__NR_getpmsg)
#define __NR_getpmsg (__NR_Linux + 208)
#endif

#if !defined(__NR_putpmsg)
#define __NR_putpmsg (__NR_Linux + 209)
#endif

#if !defined(__NR_mmap2)
#define __NR_mmap2 (__NR_Linux + 210)
#endif

#if !defined(__NR_truncate64)
#define __NR_truncate64 (__NR_Linux + 211)
#endif

#if !defined(__NR_ftruncate64)
#define __NR_ftruncate64 (__NR_Linux + 212)
#endif

#if !defined(__NR_stat64)
#define __NR_stat64 (__NR_Linux + 213)
#endif

#if !defined(__NR_lstat64)
#define __NR_lstat64 (__NR_Linux + 214)
#endif

#if !defined(__NR_fstat64)
#define __NR_fstat64 (__NR_Linux + 215)
#endif

#if !defined(__NR_pivot_root)
#define __NR_pivot_root (__NR_Linux + 216)
#endif

#if !defined(__NR_mincore)
#define __NR_mincore (__NR_Linux + 217)
#endif

#if !defined(__NR_madvise)
#define __NR_madvise (__NR_Linux + 218)
#endif

#if !defined(__NR_getdents64)
#define __NR_getdents64 (__NR_Linux + 219)
#endif

#if !defined(__NR_fcntl64)
#define __NR_fcntl64 (__NR_Linux + 220)
#endif

#if !defined(__NR_reserved221)
#define __NR_reserved221 (__NR_Linux + 221)
#endif

#if !defined(__NR_gettid)
#define __NR_gettid (__NR_Linux + 222)
#endif

#if !defined(__NR_readahead)
#define __NR_readahead (__NR_Linux + 223)
#endif

#if !defined(__NR_setxattr)
#define __NR_setxattr (__NR_Linux + 224)
#endif

#if !defined(__NR_lsetxattr)
#define __NR_lsetxattr (__NR_Linux + 225)
#endif

#if !defined(__NR_fsetxattr)
#define __NR_fsetxattr (__NR_Linux + 226)
#endif

#if !defined(__NR_getxattr)
#define __NR_getxattr (__NR_Linux + 227)
#endif

#if !defined(__NR_lgetxattr)
#define __NR_lgetxattr (__NR_Linux + 228)
#endif

#if !defined(__NR_fgetxattr)
#define __NR_fgetxattr (__NR_Linux + 229)
#endif

#if !defined(__NR_listxattr)
#define __NR_listxattr (__NR_Linux + 230)
#endif

#if !defined(__NR_llistxattr)
#define __NR_llistxattr (__NR_Linux + 231)
#endif

#if !defined(__NR_flistxattr)
#define __NR_flistxattr (__NR_Linux + 232)
#endif

#if !defined(__NR_removexattr)
#define __NR_removexattr (__NR_Linux + 233)
#endif

#if !defined(__NR_lremovexattr)
#define __NR_lremovexattr (__NR_Linux + 234)
#endif

#if !defined(__NR_fremovexattr)
#define __NR_fremovexattr (__NR_Linux + 235)
#endif

#if !defined(__NR_tkill)
#define __NR_tkill (__NR_Linux + 236)
#endif

#if !defined(__NR_sendfile64)
#define __NR_sendfile64 (__NR_Linux + 237)
#endif

#if !defined(__NR_futex)
#define __NR_futex (__NR_Linux + 238)
#endif

#if !defined(__NR_sched_setaffinity)
#define __NR_sched_setaffinity (__NR_Linux + 239)
#endif

#if !defined(__NR_sched_getaffinity)
#define __NR_sched_getaffinity (__NR_Linux + 240)
#endif

#if !defined(__NR_io_setup)
#define __NR_io_setup (__NR_Linux + 241)
#endif

#if !defined(__NR_io_destroy)
#define __NR_io_destroy (__NR_Linux + 242)
#endif

#if !defined(__NR_io_getevents)
#define __NR_io_getevents (__NR_Linux + 243)
#endif

#if !defined(__NR_io_submit)
#define __NR_io_submit (__NR_Linux + 244)
#endif

#if !defined(__NR_io_cancel)
#define __NR_io_cancel (__NR_Linux + 245)
#endif

#if !defined(__NR_exit_group)
#define __NR_exit_group (__NR_Linux + 246)
#endif

#if !defined(__NR_lookup_dcookie)
#define __NR_lookup_dcookie (__NR_Linux + 247)
#endif

#if !defined(__NR_epoll_create)
#define __NR_epoll_create (__NR_Linux + 248)
#endif

#if !defined(__NR_epoll_ctl)
#define __NR_epoll_ctl (__NR_Linux + 249)
#endif

#if !defined(__NR_epoll_wait)
#define __NR_epoll_wait (__NR_Linux + 250)
#endif

#if !defined(__NR_remap_file_pages)
#define __NR_remap_file_pages (__NR_Linux + 251)
#endif

#if !defined(__NR_set_tid_address)
#define __NR_set_tid_address (__NR_Linux + 252)
#endif

#if !defined(__NR_restart_syscall)
#define __NR_restart_syscall (__NR_Linux + 253)
#endif

#if !defined(__NR_fadvise64)
#define __NR_fadvise64 (__NR_Linux + 254)
#endif

#if !defined(__NR_statfs64)
#define __NR_statfs64 (__NR_Linux + 255)
#endif

#if !defined(__NR_fstatfs64)
#define __NR_fstatfs64 (__NR_Linux + 256)
#endif

#if !defined(__NR_timer_create)
#define __NR_timer_create (__NR_Linux + 257)
#endif

#if !defined(__NR_timer_settime)
#define __NR_timer_settime (__NR_Linux + 258)
#endif

#if !defined(__NR_timer_gettime)
#define __NR_timer_gettime (__NR_Linux + 259)
#endif

#if !defined(__NR_timer_getoverrun)
#define __NR_timer_getoverrun (__NR_Linux + 260)
#endif

#if !defined(__NR_timer_delete)
#define __NR_timer_delete (__NR_Linux + 261)
#endif

#if !defined(__NR_clock_settime)
#define __NR_clock_settime (__NR_Linux + 262)
#endif

#if !defined(__NR_clock_gettime)
#define __NR_clock_gettime (__NR_Linux + 263)
#endif

#if !defined(__NR_clock_getres)
#define __NR_clock_getres (__NR_Linux + 264)
#endif

#if !defined(__NR_clock_nanosleep)
#define __NR_clock_nanosleep (__NR_Linux + 265)
#endif

#if !defined(__NR_tgkill)
#define __NR_tgkill (__NR_Linux + 266)
#endif

#if !defined(__NR_utimes)
#define __NR_utimes (__NR_Linux + 267)
#endif

#if !defined(__NR_mbind)
#define __NR_mbind (__NR_Linux + 268)
#endif

#if !defined(__NR_get_mempolicy)
#define __NR_get_mempolicy (__NR_Linux + 269)
#endif

#if !defined(__NR_set_mempolicy)
#define __NR_set_mempolicy (__NR_Linux + 270)
#endif

#if !defined(__NR_mq_open)
#define __NR_mq_open (__NR_Linux + 271)
#endif

#if !defined(__NR_mq_unlink)
#define __NR_mq_unlink (__NR_Linux + 272)
#endif

#if !defined(__NR_mq_timedsend)
#define __NR_mq_timedsend (__NR_Linux + 273)
#endif

#if !defined(__NR_mq_timedreceive)
#define __NR_mq_timedreceive (__NR_Linux + 274)
#endif

#if !defined(__NR_mq_notify)
#define __NR_mq_notify (__NR_Linux + 275)
#endif

#if !defined(__NR_mq_getsetattr)
#define __NR_mq_getsetattr (__NR_Linux + 276)
#endif

#if !defined(__NR_vserver)
#define __NR_vserver (__NR_Linux + 277)
#endif

#if !defined(__NR_waitid)
#define __NR_waitid (__NR_Linux + 278)
#endif

/* #define __NR_sys_setaltroot (__NR_Linux + 279) */

#if !defined(__NR_add_key)
#define __NR_add_key (__NR_Linux + 280)
#endif

#if !defined(__NR_request_key)
#define __NR_request_key (__NR_Linux + 281)
#endif

#if !defined(__NR_keyctl)
#define __NR_keyctl (__NR_Linux + 282)
#endif

#if !defined(__NR_set_thread_area)
#define __NR_set_thread_area (__NR_Linux + 283)
#endif

#if !defined(__NR_inotify_init)
#define __NR_inotify_init (__NR_Linux + 284)
#endif

#if !defined(__NR_inotify_add_watch)
#define __NR_inotify_add_watch (__NR_Linux + 285)
#endif

#if !defined(__NR_inotify_rm_watch)
#define __NR_inotify_rm_watch (__NR_Linux + 286)
#endif

#if !defined(__NR_migrate_pages)
#define __NR_migrate_pages (__NR_Linux + 287)
#endif

#if !defined(__NR_openat)
#define __NR_openat (__NR_Linux + 288)
#endif

#if !defined(__NR_mkdirat)
#define __NR_mkdirat (__NR_Linux + 289)
#endif

#if !defined(__NR_mknodat)
#define __NR_mknodat (__NR_Linux + 290)
#endif

#if !defined(__NR_fchownat)
#define __NR_fchownat (__NR_Linux + 291)
#endif

#if !defined(__NR_futimesat)
#define __NR_futimesat (__NR_Linux + 292)
#endif

#if !defined(__NR_fstatat64)
#define __NR_fstatat64 (__NR_Linux + 293)
#endif

#if !defined(__NR_unlinkat)
#define __NR_unlinkat (__NR_Linux + 294)
#endif

#if !defined(__NR_renameat)
#define __NR_renameat (__NR_Linux + 295)
#endif

#if !defined(__NR_linkat)
#define __NR_linkat (__NR_Linux + 296)
#endif

#if !defined(__NR_symlinkat)
#define __NR_symlinkat (__NR_Linux + 297)
#endif

#if !defined(__NR_readlinkat)
#define __NR_readlinkat (__NR_Linux + 298)
#endif

#if !defined(__NR_fchmodat)
#define __NR_fchmodat (__NR_Linux + 299)
#endif

#if !defined(__NR_faccessat)
#define __NR_faccessat (__NR_Linux + 300)
#endif

#if !defined(__NR_pselect6)
#define __NR_pselect6 (__NR_Linux + 301)
#endif

#if !defined(__NR_ppoll)
#define __NR_ppoll (__NR_Linux + 302)
#endif

#if !defined(__NR_unshare)
#define __NR_unshare (__NR_Linux + 303)
#endif

#if !defined(__NR_splice)
#define __NR_splice (__NR_Linux + 304)
#endif

#if !defined(__NR_sync_file_range)
#define __NR_sync_file_range (__NR_Linux + 305)
#endif

#if !defined(__NR_tee)
#define __NR_tee (__NR_Linux + 306)
#endif

#if !defined(__NR_vmsplice)
#define __NR_vmsplice (__NR_Linux + 307)
#endif

#if !defined(__NR_move_pages)
#define __NR_move_pages (__NR_Linux + 308)
#endif

#if !defined(__NR_set_robust_list)
#define __NR_set_robust_list (__NR_Linux + 309)
#endif

#if !defined(__NR_get_robust_list)
#define __NR_get_robust_list (__NR_Linux + 310)
#endif

#if !defined(__NR_kexec_load)
#define __NR_kexec_load (__NR_Linux + 311)
#endif

#if !defined(__NR_getcpu)
#define __NR_getcpu (__NR_Linux + 312)
#endif

#if !defined(__NR_epoll_pwait)
#define __NR_epoll_pwait (__NR_Linux + 313)
#endif

#if !defined(__NR_ioprio_set)
#define __NR_ioprio_set (__NR_Linux + 314)
#endif

#if !defined(__NR_ioprio_get)
#define __NR_ioprio_get (__NR_Linux + 315)
#endif

#if !defined(__NR_utimensat)
#define __NR_utimensat (__NR_Linux + 316)
#endif

#if !defined(__NR_signalfd)
#define __NR_signalfd (__NR_Linux + 317)
#endif

#if !defined(__NR_timerfd)
#define __NR_timerfd (__NR_Linux + 318)
#endif

#if !defined(__NR_eventfd)
#define __NR_eventfd (__NR_Linux + 319)
#endif

#if !defined(__NR_fallocate)
#define __NR_fallocate (__NR_Linux + 320)
#endif

#if !defined(__NR_timerfd_create)
#define __NR_timerfd_create (__NR_Linux + 321)
#endif

#if !defined(__NR_timerfd_gettime)
#define __NR_timerfd_gettime (__NR_Linux + 322)
#endif

#if !defined(__NR_timerfd_settime)
#define __NR_timerfd_settime (__NR_Linux + 323)
#endif

#if !defined(__NR_signalfd4)
#define __NR_signalfd4 (__NR_Linux + 324)
#endif

#if !defined(__NR_eventfd2)
#define __NR_eventfd2 (__NR_Linux + 325)
#endif

#if !defined(__NR_epoll_create1)
#define __NR_epoll_create1 (__NR_Linux + 326)
#endif

#if !defined(__NR_dup3)
#define __NR_dup3 (__NR_Linux + 327)
#endif

#if !defined(__NR_pipe2)
#define __NR_pipe2 (__NR_Linux + 328)
#endif

#if !defined(__NR_inotify_init1)
#define __NR_inotify_init1 (__NR_Linux + 329)
#endif

#if !defined(__NR_preadv)
#define __NR_preadv (__NR_Linux + 330)
#endif

#if !defined(__NR_pwritev)
#define __NR_pwritev (__NR_Linux + 331)
#endif

#if !defined(__NR_rt_tgsigqueueinfo)
#define __NR_rt_tgsigqueueinfo (__NR_Linux + 332)
#endif

#if !defined(__NR_perf_event_open)
#define __NR_perf_event_open (__NR_Linux + 333)
#endif

#if !defined(__NR_accept4)
#define __NR_accept4 (__NR_Linux + 334)
#endif

#if !defined(__NR_recvmmsg)
#define __NR_recvmmsg (__NR_Linux + 335)
#endif

#if !defined(__NR_fanotify_init)
#define __NR_fanotify_init (__NR_Linux + 336)
#endif

#if !defined(__NR_fanotify_mark)
#define __NR_fanotify_mark (__NR_Linux + 337)
#endif

#if !defined(__NR_prlimit64)
#define __NR_prlimit64 (__NR_Linux + 338)
#endif

#if !defined(__NR_name_to_handle_at)
#define __NR_name_to_handle_at (__NR_Linux + 339)
#endif

#if !defined(__NR_open_by_handle_at)
#define __NR_open_by_handle_at (__NR_Linux + 340)
#endif

#if !defined(__NR_clock_adjtime)
#define __NR_clock_adjtime (__NR_Linux + 341)
#endif

#if !defined(__NR_syncfs)
#define __NR_syncfs (__NR_Linux + 342)
#endif

#if !defined(__NR_sendmmsg)
#define __NR_sendmmsg (__NR_Linux + 343)
#endif

#if !defined(__NR_setns)
#define __NR_setns (__NR_Linux + 344)
#endif

#if !defined(__NR_process_vm_readv)
#define __NR_process_vm_readv (__NR_Linux + 345)
#endif

#if !defined(__NR_process_vm_writev)
#define __NR_process_vm_writev (__NR_Linux + 346)
#endif

#if !defined(__NR_kcmp)
#define __NR_kcmp (__NR_Linux + 347)
#endif

#if !defined(__NR_finit_module)
#define __NR_finit_module (__NR_Linux + 348)
#endif

#if !defined(__NR_sched_setattr)
#define __NR_sched_setattr (__NR_Linux + 349)
#endif

#if !defined(__NR_sched_getattr)
#define __NR_sched_getattr (__NR_Linux + 350)
#endif

#if !defined(__NR_renameat2)
#define __NR_renameat2 (__NR_Linux + 351)
#endif

#if !defined(__NR_seccomp)
#define __NR_seccomp (__NR_Linux + 352)
#endif

#if !defined(__NR_getrandom)
#define __NR_getrandom (__NR_Linux + 353)
#endif

#if !defined(__NR_memfd_create)
#define __NR_memfd_create (__NR_Linux + 354)
#endif

#if !defined(__NR_bpf)
#define __NR_bpf (__NR_Linux + 355)
#endif

#if !defined(__NR_execveat)
#define __NR_execveat (__NR_Linux + 356)
#endif

#if !defined(__NR_userfaultfd)
#define __NR_userfaultfd (__NR_Linux + 357)
#endif

#if !defined(__NR_membarrier)
#define __NR_membarrier (__NR_Linux + 358)
#endif

#if !defined(__NR_mlock2)
#define __NR_mlock2 (__NR_Linux + 359)
#endif

#if !defined(__NR_copy_file_range)
#define __NR_copy_file_range (__NR_Linux + 360)
#endif

#if !defined(__NR_preadv2)
#define __NR_preadv2 (__NR_Linux + 361)
#endif

#if !defined(__NR_pwritev2)
#define __NR_pwritev2 (__NR_Linux + 362)
#endif

#if !defined(__NR_pkey_mprotect)
#define __NR_pkey_mprotect (__NR_Linux + 363)
#endif

#if !defined(__NR_pkey_alloc)
#define __NR_pkey_alloc (__NR_Linux + 364)
#endif

#if !defined(__NR_pkey_free)
#define __NR_pkey_free (__NR_Linux + 365)
#endif

#if !defined(__NR_statx)
#define __NR_statx (__NR_Linux + 366)
#endif

#if !defined(__NR_rseq)
#define __NR_rseq (__NR_Linux + 367)
#endif

#if !defined(__NR_io_pgetevents)
#define __NR_io_pgetevents (__NR_Linux + 368)
#endif

#if !defined(__NR_semget)
#define __NR_semget (__NR_Linux + 393)
#endif

#if !defined(__NR_semctl)
#define __NR_semctl (__NR_Linux + 394)
#endif

#if !defined(__NR_shmget)
#define __NR_shmget (__NR_Linux + 395)
#endif

#if !defined(__NR_shmctl)
#define __NR_shmctl (__NR_Linux + 396)
#endif

#if !defined(__NR_shmat)
#define __NR_shmat (__NR_Linux + 397)
#endif

#if !defined(__NR_shmdt)
#define __NR_shmdt (__NR_Linux + 398)
#endif

#if !defined(__NR_msgget)
#define __NR_msgget (__NR_Linux + 399)
#endif

#if !defined(__NR_msgsnd)
#define __NR_msgsnd (__NR_Linux + 400)
#endif

#if !defined(__NR_msgrcv)
#define __NR_msgrcv (__NR_Linux + 401)
#endif

#if !defined(__NR_msgctl)
#define __NR_msgctl (__NR_Linux + 402)
#endif

#if !defined(__NR_clock_gettime64)
#define __NR_clock_gettime64 (__NR_Linux + 403)
#endif

#if !defined(__NR_clock_settime64)
#define __NR_clock_settime64 (__NR_Linux + 404)
#endif

#if !defined(__NR_clock_adjtime64)
#define __NR_clock_adjtime64 (__NR_Linux + 405)
#endif

#if !defined(__NR_clock_getres_time64)
#define __NR_clock_getres_time64 (__NR_Linux + 406)
#endif

#if !defined(__NR_clock_nanosleep_time64)
#define __NR_clock_nanosleep_time64 (__NR_Linux + 407)
#endif

#if !defined(__NR_timer_gettime64)
#define __NR_timer_gettime64 (__NR_Linux + 408)
#endif

#if !defined(__NR_timer_settime64)
#define __NR_timer_settime64 (__NR_Linux + 409)
#endif

#if !defined(__NR_timerfd_gettime64)
#define __NR_timerfd_gettime64 (__NR_Linux + 410)
#endif

#if !defined(__NR_timerfd_settime64)
#define __NR_timerfd_settime64 (__NR_Linux + 411)
#endif

#if !defined(__NR_utimensat_time64)
#define __NR_utimensat_time64 (__NR_Linux + 412)
#endif

#if !defined(__NR_pselect6_time64)
#define __NR_pselect6_time64 (__NR_Linux + 413)
#endif

#if !defined(__NR_ppoll_time64)
#define __NR_ppoll_time64 (__NR_Linux + 414)
#endif

#if !defined(__NR_io_pgetevents_time64)
#define __NR_io_pgetevents_time64 (__NR_Linux + 416)
#endif

#if !defined(__NR_recvmmsg_time64)
#define __NR_recvmmsg_time64 (__NR_Linux + 417)
#endif

#if !defined(__NR_mq_timedsend_time64)
#define __NR_mq_timedsend_time64 (__NR_Linux + 418)
#endif

#if !defined(__NR_mq_timedreceive_time64)
#define __NR_mq_timedreceive_time64 (__NR_Linux + 419)
#endif

#if !defined(__NR_semtimedop_time64)
#define __NR_semtimedop_time64 (__NR_Linux + 420)
#endif

#if !defined(__NR_rt_sigtimedwait_time64)
#define __NR_rt_sigtimedwait_time64 (__NR_Linux + 421)
#endif

#if !defined(__NR_futex_time64)
#define __NR_futex_time64 (__NR_Linux + 422)
#endif

#if !defined(__NR_sched_rr_get_interval_time64)
#define __NR_sched_rr_get_interval_time64 (__NR_Linux + 423)
#endif

#if !defined(__NR_pidfd_send_signal)
#define __NR_pidfd_send_signal (__NR_Linux + 424)
#endif

#if !defined(__NR_io_uring_setup)
#define __NR_io_uring_setup (__NR_Linux + 425)
#endif

#if !defined(__NR_io_uring_enter)
#define __NR_io_uring_enter (__NR_Linux + 426)
#endif

#if !defined(__NR_io_uring_register)
#define __NR_io_uring_register (__NR_Linux + 427)
#endif

#if !defined(__NR_open_tree)
#define __NR_open_tree (__NR_Linux + 428)
#endif

#if !defined(__NR_move_mount)
#define __NR_move_mount (__NR_Linux + 429)
#endif

#if !defined(__NR_fsopen)
#define __NR_fsopen (__NR_Linux + 430)
#endif

#if !defined(__NR_fsconfig)
#define __NR_fsconfig (__NR_Linux + 431)
#endif

#if !defined(__NR_fsmount)
#define __NR_fsmount (__NR_Linux + 432)
#endif

#if !defined(__NR_fspick)
#define __NR_fspick (__NR_Linux + 433)
#endif

#if !defined(__NR_pidfd_open)
#define __NR_pidfd_open (__NR_Linux + 434)
#endif

#if !defined(__NR_clone3)
#define __NR_clone3 (__NR_Linux + 435)
#endif

#if !defined(__NR_close_range)
#define __NR_close_range (__NR_Linux + 436)
#endif

#if !defined(__NR_openat2)
#define __NR_openat2 (__NR_Linux + 437)
#endif

#if !defined(__NR_pidfd_getfd)
#define __NR_pidfd_getfd (__NR_Linux + 438)
#endif

#if !defined(__NR_faccessat2)
#define __NR_faccessat2 (__NR_Linux + 439)
#endif

#if !defined(__NR_process_madvise)
#define __NR_process_madvise (__NR_Linux + 440)
#endif

#if !defined(__NR_epoll_pwait2)
#define __NR_epoll_pwait2 (__NR_Linux + 441)
#endif

#if !defined(__NR_mount_setattr)
#define __NR_mount_setattr (__NR_Linux + 442)
#endif

#if !defined(__NR_landlock_create_ruleset)
#define __NR_landlock_create_ruleset (__NR_Linux + 444)
#endif

#if !defined(__NR_landlock_add_rule)
#define __NR_landlock_add_rule (__NR_Linux + 445)
#endif

#if !defined(__NR_landlock_restrict_self)
#define __NR_landlock_restrict_self (__NR_Linux + 446)
#endif

#if !defined(__NR_process_mrelease)
#define __NR_process_mrelease (__NR_Linux + 448)
#endif

#if !defined(__NR_futex_waitv)
#define __NR_futex_waitv (__NR_Linux + 449)
#endif

#if !defined(__NR_set_mempolicy_home_node)
#define __NR_set_mempolicy_home_node (__NR_Linux + 450)
#endif

#if !defined(__NR_cachestat)
#define __NR_cachestat (__NR_Linux + 451)
#endif

#if !defined(__NR_fchmodat2)
#define __NR_fchmodat2 (__NR_Linux + 452)
#endif

#if !defined(__NR_map_shadow_stack)
#define __NR_map_shadow_stack (__NR_Linux + 453)
#endif

#if !defined(__NR_futex_wake)
#define __NR_futex_wake (__NR_Linux + 454)
#endif

#if !defined(__NR_futex_wait)
#define __NR_futex_wait (__NR_Linux + 455)
#endif

#if !defined(__NR_futex_requeue)
#define __NR_futex_requeue (__NR_Linux + 456)
#endif

#if !defined(__NR_statmount)
#define __NR_statmount (__NR_Linux + 457)
#endif

#if !defined(__NR_listmount)
#define __NR_listmount (__NR_Linux + 458)
#endif

#if !defined(__NR_lsm_get_self_attr)
#define __NR_lsm_get_self_attr (__NR_Linux + 459)
#endif

#if !defined(__NR_lsm_set_self_attr)
#define __NR_lsm_set_self_attr (__NR_Linux + 460)
#endif

#if !defined(__NR_lsm_list_modules)
#define __NR_lsm_list_modules (__NR_Linux + 461)
#endif

#if !defined(__NR_mseal)
#define __NR_mseal (__NR_Linux + 462)
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_MIPS_LINUX_SYSCALLS_H_
