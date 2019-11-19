// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generated from the Linux kernel's syscall_32.tbl.
#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_X86_32_LINUX_SYSCALLS_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_X86_32_LINUX_SYSCALLS_H_

#if !defined(__i386__)
#error "Including header on wrong architecture"
#endif

#if !defined(__NR_restart_syscall)
#define __NR_restart_syscall 0
#endif

#if !defined(__NR_exit)
#define __NR_exit 1
#endif

#if !defined(__NR_fork)
#define __NR_fork 2
#endif

#if !defined(__NR_read)
#define __NR_read 3
#endif

#if !defined(__NR_write)
#define __NR_write 4
#endif

#if !defined(__NR_open)
#define __NR_open 5
#endif

#if !defined(__NR_close)
#define __NR_close 6
#endif

#if !defined(__NR_waitpid)
#define __NR_waitpid 7
#endif

#if !defined(__NR_creat)
#define __NR_creat 8
#endif

#if !defined(__NR_link)
#define __NR_link 9
#endif

#if !defined(__NR_unlink)
#define __NR_unlink 10
#endif

#if !defined(__NR_execve)
#define __NR_execve 11
#endif

#if !defined(__NR_chdir)
#define __NR_chdir 12
#endif

#if !defined(__NR_time)
#define __NR_time 13
#endif

#if !defined(__NR_mknod)
#define __NR_mknod 14
#endif

#if !defined(__NR_chmod)
#define __NR_chmod 15
#endif

#if !defined(__NR_lchown)
#define __NR_lchown 16
#endif

#if !defined(__NR_break)
#define __NR_break 17
#endif

#if !defined(__NR_oldstat)
#define __NR_oldstat 18
#endif

#if !defined(__NR_lseek)
#define __NR_lseek 19
#endif

#if !defined(__NR_getpid)
#define __NR_getpid 20
#endif

#if !defined(__NR_mount)
#define __NR_mount 21
#endif

#if !defined(__NR_umount)
#define __NR_umount 22
#endif

#if !defined(__NR_setuid)
#define __NR_setuid 23
#endif

#if !defined(__NR_getuid)
#define __NR_getuid 24
#endif

#if !defined(__NR_stime)
#define __NR_stime 25
#endif

#if !defined(__NR_ptrace)
#define __NR_ptrace 26
#endif

#if !defined(__NR_alarm)
#define __NR_alarm 27
#endif

#if !defined(__NR_oldfstat)
#define __NR_oldfstat 28
#endif

#if !defined(__NR_pause)
#define __NR_pause 29
#endif

#if !defined(__NR_utime)
#define __NR_utime 30
#endif

#if !defined(__NR_stty)
#define __NR_stty 31
#endif

#if !defined(__NR_gtty)
#define __NR_gtty 32
#endif

#if !defined(__NR_access)
#define __NR_access 33
#endif

#if !defined(__NR_nice)
#define __NR_nice 34
#endif

#if !defined(__NR_ftime)
#define __NR_ftime 35
#endif

#if !defined(__NR_sync)
#define __NR_sync 36
#endif

#if !defined(__NR_kill)
#define __NR_kill 37
#endif

#if !defined(__NR_rename)
#define __NR_rename 38
#endif

#if !defined(__NR_mkdir)
#define __NR_mkdir 39
#endif

#if !defined(__NR_rmdir)
#define __NR_rmdir 40
#endif

#if !defined(__NR_dup)
#define __NR_dup 41
#endif

#if !defined(__NR_pipe)
#define __NR_pipe 42
#endif

#if !defined(__NR_times)
#define __NR_times 43
#endif

#if !defined(__NR_prof)
#define __NR_prof 44
#endif

#if !defined(__NR_brk)
#define __NR_brk 45
#endif

#if !defined(__NR_setgid)
#define __NR_setgid 46
#endif

#if !defined(__NR_getgid)
#define __NR_getgid 47
#endif

#if !defined(__NR_signal)
#define __NR_signal 48
#endif

#if !defined(__NR_geteuid)
#define __NR_geteuid 49
#endif

#if !defined(__NR_getegid)
#define __NR_getegid 50
#endif

#if !defined(__NR_acct)
#define __NR_acct 51
#endif

#if !defined(__NR_umount2)
#define __NR_umount2 52
#endif

#if !defined(__NR_lock)
#define __NR_lock 53
#endif

#if !defined(__NR_ioctl)
#define __NR_ioctl 54
#endif

#if !defined(__NR_fcntl)
#define __NR_fcntl 55
#endif

#if !defined(__NR_mpx)
#define __NR_mpx 56
#endif

#if !defined(__NR_setpgid)
#define __NR_setpgid 57
#endif

#if !defined(__NR_ulimit)
#define __NR_ulimit 58
#endif

#if !defined(__NR_oldolduname)
#define __NR_oldolduname 59
#endif

#if !defined(__NR_umask)
#define __NR_umask 60
#endif

#if !defined(__NR_chroot)
#define __NR_chroot 61
#endif

#if !defined(__NR_ustat)
#define __NR_ustat 62
#endif

#if !defined(__NR_dup2)
#define __NR_dup2 63
#endif

#if !defined(__NR_getppid)
#define __NR_getppid 64
#endif

#if !defined(__NR_getpgrp)
#define __NR_getpgrp 65
#endif

#if !defined(__NR_setsid)
#define __NR_setsid 66
#endif

#if !defined(__NR_sigaction)
#define __NR_sigaction 67
#endif

#if !defined(__NR_sgetmask)
#define __NR_sgetmask 68
#endif

#if !defined(__NR_ssetmask)
#define __NR_ssetmask 69
#endif

#if !defined(__NR_setreuid)
#define __NR_setreuid 70
#endif

#if !defined(__NR_setregid)
#define __NR_setregid 71
#endif

#if !defined(__NR_sigsuspend)
#define __NR_sigsuspend 72
#endif

#if !defined(__NR_sigpending)
#define __NR_sigpending 73
#endif

#if !defined(__NR_sethostname)
#define __NR_sethostname 74
#endif

#if !defined(__NR_setrlimit)
#define __NR_setrlimit 75
#endif

#if !defined(__NR_getrlimit)
#define __NR_getrlimit 76
#endif

#if !defined(__NR_getrusage)
#define __NR_getrusage 77
#endif

#if !defined(__NR_gettimeofday)
#define __NR_gettimeofday 78
#endif

#if !defined(__NR_settimeofday)
#define __NR_settimeofday 79
#endif

#if !defined(__NR_getgroups)
#define __NR_getgroups 80
#endif

#if !defined(__NR_setgroups)
#define __NR_setgroups 81
#endif

#if !defined(__NR_select)
#define __NR_select 82
#endif

#if !defined(__NR_symlink)
#define __NR_symlink 83
#endif

#if !defined(__NR_oldlstat)
#define __NR_oldlstat 84
#endif

#if !defined(__NR_readlink)
#define __NR_readlink 85
#endif

#if !defined(__NR_uselib)
#define __NR_uselib 86
#endif

#if !defined(__NR_swapon)
#define __NR_swapon 87
#endif

#if !defined(__NR_reboot)
#define __NR_reboot 88
#endif

#if !defined(__NR_readdir)
#define __NR_readdir 89
#endif

#if !defined(__NR_mmap)
#define __NR_mmap 90
#endif

#if !defined(__NR_munmap)
#define __NR_munmap 91
#endif

#if !defined(__NR_truncate)
#define __NR_truncate 92
#endif

#if !defined(__NR_ftruncate)
#define __NR_ftruncate 93
#endif

#if !defined(__NR_fchmod)
#define __NR_fchmod 94
#endif

#if !defined(__NR_fchown)
#define __NR_fchown 95
#endif

#if !defined(__NR_getpriority)
#define __NR_getpriority 96
#endif

#if !defined(__NR_setpriority)
#define __NR_setpriority 97
#endif

#if !defined(__NR_profil)
#define __NR_profil 98
#endif

#if !defined(__NR_statfs)
#define __NR_statfs 99
#endif

#if !defined(__NR_fstatfs)
#define __NR_fstatfs 100
#endif

#if !defined(__NR_ioperm)
#define __NR_ioperm 101
#endif

#if !defined(__NR_socketcall)
#define __NR_socketcall 102
#endif

#if !defined(__NR_syslog)
#define __NR_syslog 103
#endif

#if !defined(__NR_setitimer)
#define __NR_setitimer 104
#endif

#if !defined(__NR_getitimer)
#define __NR_getitimer 105
#endif

#if !defined(__NR_stat)
#define __NR_stat 106
#endif

#if !defined(__NR_lstat)
#define __NR_lstat 107
#endif

#if !defined(__NR_fstat)
#define __NR_fstat 108
#endif

#if !defined(__NR_olduname)
#define __NR_olduname 109
#endif

#if !defined(__NR_iopl)
#define __NR_iopl 110
#endif

#if !defined(__NR_vhangup)
#define __NR_vhangup 111
#endif

#if !defined(__NR_idle)
#define __NR_idle 112
#endif

#if !defined(__NR_vm86old)
#define __NR_vm86old 113
#endif

#if !defined(__NR_wait4)
#define __NR_wait4 114
#endif

#if !defined(__NR_swapoff)
#define __NR_swapoff 115
#endif

#if !defined(__NR_sysinfo)
#define __NR_sysinfo 116
#endif

#if !defined(__NR_ipc)
#define __NR_ipc 117
#endif

#if !defined(__NR_fsync)
#define __NR_fsync 118
#endif

#if !defined(__NR_sigreturn)
#define __NR_sigreturn 119
#endif

#if !defined(__NR_clone)
#define __NR_clone 120
#endif

#if !defined(__NR_setdomainname)
#define __NR_setdomainname 121
#endif

#if !defined(__NR_uname)
#define __NR_uname 122
#endif

#if !defined(__NR_modify_ldt)
#define __NR_modify_ldt 123
#endif

#if !defined(__NR_adjtimex)
#define __NR_adjtimex 124
#endif

#if !defined(__NR_mprotect)
#define __NR_mprotect 125
#endif

#if !defined(__NR_sigprocmask)
#define __NR_sigprocmask 126
#endif

#if !defined(__NR_create_module)
#define __NR_create_module 127
#endif

#if !defined(__NR_init_module)
#define __NR_init_module 128
#endif

#if !defined(__NR_delete_module)
#define __NR_delete_module 129
#endif

#if !defined(__NR_get_kernel_syms)
#define __NR_get_kernel_syms 130
#endif

#if !defined(__NR_quotactl)
#define __NR_quotactl 131
#endif

#if !defined(__NR_getpgid)
#define __NR_getpgid 132
#endif

#if !defined(__NR_fchdir)
#define __NR_fchdir 133
#endif

#if !defined(__NR_bdflush)
#define __NR_bdflush 134
#endif

#if !defined(__NR_sysfs)
#define __NR_sysfs 135
#endif

#if !defined(__NR_personality)
#define __NR_personality 136
#endif

#if !defined(__NR_afs_syscall)
#define __NR_afs_syscall 137
#endif

#if !defined(__NR_setfsuid)
#define __NR_setfsuid 138
#endif

#if !defined(__NR_setfsgid)
#define __NR_setfsgid 139
#endif

#if !defined(__NR__llseek)
#define __NR__llseek 140
#endif

#if !defined(__NR_getdents)
#define __NR_getdents 141
#endif

#if !defined(__NR__newselect)
#define __NR__newselect 142
#endif

#if !defined(__NR_flock)
#define __NR_flock 143
#endif

#if !defined(__NR_msync)
#define __NR_msync 144
#endif

#if !defined(__NR_readv)
#define __NR_readv 145
#endif

#if !defined(__NR_writev)
#define __NR_writev 146
#endif

#if !defined(__NR_getsid)
#define __NR_getsid 147
#endif

#if !defined(__NR_fdatasync)
#define __NR_fdatasync 148
#endif

#if !defined(__NR__sysctl)
#define __NR__sysctl 149
#endif

#if !defined(__NR_mlock)
#define __NR_mlock 150
#endif

#if !defined(__NR_munlock)
#define __NR_munlock 151
#endif

#if !defined(__NR_mlockall)
#define __NR_mlockall 152
#endif

#if !defined(__NR_munlockall)
#define __NR_munlockall 153
#endif

#if !defined(__NR_sched_setparam)
#define __NR_sched_setparam 154
#endif

#if !defined(__NR_sched_getparam)
#define __NR_sched_getparam 155
#endif

#if !defined(__NR_sched_setscheduler)
#define __NR_sched_setscheduler 156
#endif

#if !defined(__NR_sched_getscheduler)
#define __NR_sched_getscheduler 157
#endif

#if !defined(__NR_sched_yield)
#define __NR_sched_yield 158
#endif

#if !defined(__NR_sched_get_priority_max)
#define __NR_sched_get_priority_max 159
#endif

#if !defined(__NR_sched_get_priority_min)
#define __NR_sched_get_priority_min 160
#endif

#if !defined(__NR_sched_rr_get_interval)
#define __NR_sched_rr_get_interval 161
#endif

#if !defined(__NR_nanosleep)
#define __NR_nanosleep 162
#endif

#if !defined(__NR_mremap)
#define __NR_mremap 163
#endif

#if !defined(__NR_setresuid)
#define __NR_setresuid 164
#endif

#if !defined(__NR_getresuid)
#define __NR_getresuid 165
#endif

#if !defined(__NR_vm86)
#define __NR_vm86 166
#endif

#if !defined(__NR_query_module)
#define __NR_query_module 167
#endif

#if !defined(__NR_poll)
#define __NR_poll 168
#endif

#if !defined(__NR_nfsservctl)
#define __NR_nfsservctl 169
#endif

#if !defined(__NR_setresgid)
#define __NR_setresgid 170
#endif

#if !defined(__NR_getresgid)
#define __NR_getresgid 171
#endif

#if !defined(__NR_prctl)
#define __NR_prctl 172
#endif

#if !defined(__NR_rt_sigreturn)
#define __NR_rt_sigreturn 173
#endif

#if !defined(__NR_rt_sigaction)
#define __NR_rt_sigaction 174
#endif

#if !defined(__NR_rt_sigprocmask)
#define __NR_rt_sigprocmask 175
#endif

#if !defined(__NR_rt_sigpending)
#define __NR_rt_sigpending 176
#endif

#if !defined(__NR_rt_sigtimedwait)
#define __NR_rt_sigtimedwait 177
#endif

#if !defined(__NR_rt_sigqueueinfo)
#define __NR_rt_sigqueueinfo 178
#endif

#if !defined(__NR_rt_sigsuspend)
#define __NR_rt_sigsuspend 179
#endif

#if !defined(__NR_pread64)
#define __NR_pread64 180
#endif

#if !defined(__NR_pwrite64)
#define __NR_pwrite64 181
#endif

#if !defined(__NR_chown)
#define __NR_chown 182
#endif

#if !defined(__NR_getcwd)
#define __NR_getcwd 183
#endif

#if !defined(__NR_capget)
#define __NR_capget 184
#endif

#if !defined(__NR_capset)
#define __NR_capset 185
#endif

#if !defined(__NR_sigaltstack)
#define __NR_sigaltstack 186
#endif

#if !defined(__NR_sendfile)
#define __NR_sendfile 187
#endif

#if !defined(__NR_getpmsg)
#define __NR_getpmsg 188
#endif

#if !defined(__NR_putpmsg)
#define __NR_putpmsg 189
#endif

#if !defined(__NR_vfork)
#define __NR_vfork 190
#endif

#if !defined(__NR_ugetrlimit)
#define __NR_ugetrlimit 191
#endif

#if !defined(__NR_mmap2)
#define __NR_mmap2 192
#endif

#if !defined(__NR_truncate64)
#define __NR_truncate64 193
#endif

#if !defined(__NR_ftruncate64)
#define __NR_ftruncate64 194
#endif

#if !defined(__NR_stat64)
#define __NR_stat64 195
#endif

#if !defined(__NR_lstat64)
#define __NR_lstat64 196
#endif

#if !defined(__NR_fstat64)
#define __NR_fstat64 197
#endif

#if !defined(__NR_lchown32)
#define __NR_lchown32 198
#endif

#if !defined(__NR_getuid32)
#define __NR_getuid32 199
#endif

#if !defined(__NR_getgid32)
#define __NR_getgid32 200
#endif

#if !defined(__NR_geteuid32)
#define __NR_geteuid32 201
#endif

#if !defined(__NR_getegid32)
#define __NR_getegid32 202
#endif

#if !defined(__NR_setreuid32)
#define __NR_setreuid32 203
#endif

#if !defined(__NR_setregid32)
#define __NR_setregid32 204
#endif

#if !defined(__NR_getgroups32)
#define __NR_getgroups32 205
#endif

#if !defined(__NR_setgroups32)
#define __NR_setgroups32 206
#endif

#if !defined(__NR_fchown32)
#define __NR_fchown32 207
#endif

#if !defined(__NR_setresuid32)
#define __NR_setresuid32 208
#endif

#if !defined(__NR_getresuid32)
#define __NR_getresuid32 209
#endif

#if !defined(__NR_setresgid32)
#define __NR_setresgid32 210
#endif

#if !defined(__NR_getresgid32)
#define __NR_getresgid32 211
#endif

#if !defined(__NR_chown32)
#define __NR_chown32 212
#endif

#if !defined(__NR_setuid32)
#define __NR_setuid32 213
#endif

#if !defined(__NR_setgid32)
#define __NR_setgid32 214
#endif

#if !defined(__NR_setfsuid32)
#define __NR_setfsuid32 215
#endif

#if !defined(__NR_setfsgid32)
#define __NR_setfsgid32 216
#endif

#if !defined(__NR_pivot_root)
#define __NR_pivot_root 217
#endif

#if !defined(__NR_mincore)
#define __NR_mincore 218
#endif

#if !defined(__NR_madvise)
#define __NR_madvise 219
#endif

#if !defined(__NR_getdents64)
#define __NR_getdents64 220
#endif

#if !defined(__NR_fcntl64)
#define __NR_fcntl64 221
#endif

#if !defined(__NR_gettid)
#define __NR_gettid 224
#endif

#if !defined(__NR_readahead)
#define __NR_readahead 225
#endif

#if !defined(__NR_setxattr)
#define __NR_setxattr 226
#endif

#if !defined(__NR_lsetxattr)
#define __NR_lsetxattr 227
#endif

#if !defined(__NR_fsetxattr)
#define __NR_fsetxattr 228
#endif

#if !defined(__NR_getxattr)
#define __NR_getxattr 229
#endif

#if !defined(__NR_lgetxattr)
#define __NR_lgetxattr 230
#endif

#if !defined(__NR_fgetxattr)
#define __NR_fgetxattr 231
#endif

#if !defined(__NR_listxattr)
#define __NR_listxattr 232
#endif

#if !defined(__NR_llistxattr)
#define __NR_llistxattr 233
#endif

#if !defined(__NR_flistxattr)
#define __NR_flistxattr 234
#endif

#if !defined(__NR_removexattr)
#define __NR_removexattr 235
#endif

#if !defined(__NR_lremovexattr)
#define __NR_lremovexattr 236
#endif

#if !defined(__NR_fremovexattr)
#define __NR_fremovexattr 237
#endif

#if !defined(__NR_tkill)
#define __NR_tkill 238
#endif

#if !defined(__NR_sendfile64)
#define __NR_sendfile64 239
#endif

#if !defined(__NR_futex)
#define __NR_futex 240
#endif

#if !defined(__NR_sched_setaffinity)
#define __NR_sched_setaffinity 241
#endif

#if !defined(__NR_sched_getaffinity)
#define __NR_sched_getaffinity 242
#endif

#if !defined(__NR_set_thread_area)
#define __NR_set_thread_area 243
#endif

#if !defined(__NR_get_thread_area)
#define __NR_get_thread_area 244
#endif

#if !defined(__NR_io_setup)
#define __NR_io_setup 245
#endif

#if !defined(__NR_io_destroy)
#define __NR_io_destroy 246
#endif

#if !defined(__NR_io_getevents)
#define __NR_io_getevents 247
#endif

#if !defined(__NR_io_submit)
#define __NR_io_submit 248
#endif

#if !defined(__NR_io_cancel)
#define __NR_io_cancel 249
#endif

#if !defined(__NR_fadvise64)
#define __NR_fadvise64 250
#endif

#if !defined(__NR_exit_group)
#define __NR_exit_group 252
#endif

#if !defined(__NR_lookup_dcookie)
#define __NR_lookup_dcookie 253
#endif

#if !defined(__NR_epoll_create)
#define __NR_epoll_create 254
#endif

#if !defined(__NR_epoll_ctl)
#define __NR_epoll_ctl 255
#endif

#if !defined(__NR_epoll_wait)
#define __NR_epoll_wait 256
#endif

#if !defined(__NR_remap_file_pages)
#define __NR_remap_file_pages 257
#endif

#if !defined(__NR_set_tid_address)
#define __NR_set_tid_address 258
#endif

#if !defined(__NR_timer_create)
#define __NR_timer_create 259
#endif

#if !defined(__NR_timer_settime)
#define __NR_timer_settime 260
#endif

#if !defined(__NR_timer_gettime)
#define __NR_timer_gettime 261
#endif

#if !defined(__NR_timer_getoverrun)
#define __NR_timer_getoverrun 262
#endif

#if !defined(__NR_timer_delete)
#define __NR_timer_delete 263
#endif

#if !defined(__NR_clock_settime)
#define __NR_clock_settime 264
#endif

#if !defined(__NR_clock_gettime)
#define __NR_clock_gettime 265
#endif

#if !defined(__NR_clock_getres)
#define __NR_clock_getres 266
#endif

#if !defined(__NR_clock_nanosleep)
#define __NR_clock_nanosleep 267
#endif

#if !defined(__NR_statfs64)
#define __NR_statfs64 268
#endif

#if !defined(__NR_fstatfs64)
#define __NR_fstatfs64 269
#endif

#if !defined(__NR_tgkill)
#define __NR_tgkill 270
#endif

#if !defined(__NR_utimes)
#define __NR_utimes 271
#endif

#if !defined(__NR_fadvise64_64)
#define __NR_fadvise64_64 272
#endif

#if !defined(__NR_vserver)
#define __NR_vserver 273
#endif

#if !defined(__NR_mbind)
#define __NR_mbind 274
#endif

#if !defined(__NR_get_mempolicy)
#define __NR_get_mempolicy 275
#endif

#if !defined(__NR_set_mempolicy)
#define __NR_set_mempolicy 276
#endif

#if !defined(__NR_mq_open)
#define __NR_mq_open 277
#endif

#if !defined(__NR_mq_unlink)
#define __NR_mq_unlink 278
#endif

#if !defined(__NR_mq_timedsend)
#define __NR_mq_timedsend 279
#endif

#if !defined(__NR_mq_timedreceive)
#define __NR_mq_timedreceive 280
#endif

#if !defined(__NR_mq_notify)
#define __NR_mq_notify 281
#endif

#if !defined(__NR_mq_getsetattr)
#define __NR_mq_getsetattr 282
#endif

#if !defined(__NR_kexec_load)
#define __NR_kexec_load 283
#endif

#if !defined(__NR_waitid)
#define __NR_waitid 284
#endif

#if !defined(__NR_add_key)
#define __NR_add_key 286
#endif

#if !defined(__NR_request_key)
#define __NR_request_key 287
#endif

#if !defined(__NR_keyctl)
#define __NR_keyctl 288
#endif

#if !defined(__NR_ioprio_set)
#define __NR_ioprio_set 289
#endif

#if !defined(__NR_ioprio_get)
#define __NR_ioprio_get 290
#endif

#if !defined(__NR_inotify_init)
#define __NR_inotify_init 291
#endif

#if !defined(__NR_inotify_add_watch)
#define __NR_inotify_add_watch 292
#endif

#if !defined(__NR_inotify_rm_watch)
#define __NR_inotify_rm_watch 293
#endif

#if !defined(__NR_migrate_pages)
#define __NR_migrate_pages 294
#endif

#if !defined(__NR_openat)
#define __NR_openat 295
#endif

#if !defined(__NR_mkdirat)
#define __NR_mkdirat 296
#endif

#if !defined(__NR_mknodat)
#define __NR_mknodat 297
#endif

#if !defined(__NR_fchownat)
#define __NR_fchownat 298
#endif

#if !defined(__NR_futimesat)
#define __NR_futimesat 299
#endif

#if !defined(__NR_fstatat64)
#define __NR_fstatat64 300
#endif

#if !defined(__NR_unlinkat)
#define __NR_unlinkat 301
#endif

#if !defined(__NR_renameat)
#define __NR_renameat 302
#endif

#if !defined(__NR_linkat)
#define __NR_linkat 303
#endif

#if !defined(__NR_symlinkat)
#define __NR_symlinkat 304
#endif

#if !defined(__NR_readlinkat)
#define __NR_readlinkat 305
#endif

#if !defined(__NR_fchmodat)
#define __NR_fchmodat 306
#endif

#if !defined(__NR_faccessat)
#define __NR_faccessat 307
#endif

#if !defined(__NR_pselect6)
#define __NR_pselect6 308
#endif

#if !defined(__NR_ppoll)
#define __NR_ppoll 309
#endif

#if !defined(__NR_unshare)
#define __NR_unshare 310
#endif

#if !defined(__NR_set_robust_list)
#define __NR_set_robust_list 311
#endif

#if !defined(__NR_get_robust_list)
#define __NR_get_robust_list 312
#endif

#if !defined(__NR_splice)
#define __NR_splice 313
#endif

#if !defined(__NR_sync_file_range)
#define __NR_sync_file_range 314
#endif

#if !defined(__NR_tee)
#define __NR_tee 315
#endif

#if !defined(__NR_vmsplice)
#define __NR_vmsplice 316
#endif

#if !defined(__NR_move_pages)
#define __NR_move_pages 317
#endif

#if !defined(__NR_getcpu)
#define __NR_getcpu 318
#endif

#if !defined(__NR_epoll_pwait)
#define __NR_epoll_pwait 319
#endif

#if !defined(__NR_utimensat)
#define __NR_utimensat 320
#endif

#if !defined(__NR_signalfd)
#define __NR_signalfd 321
#endif

#if !defined(__NR_timerfd_create)
#define __NR_timerfd_create 322
#endif

#if !defined(__NR_eventfd)
#define __NR_eventfd 323
#endif

#if !defined(__NR_fallocate)
#define __NR_fallocate 324
#endif

#if !defined(__NR_timerfd_settime)
#define __NR_timerfd_settime 325
#endif

#if !defined(__NR_timerfd_gettime)
#define __NR_timerfd_gettime 326
#endif

#if !defined(__NR_signalfd4)
#define __NR_signalfd4 327
#endif

#if !defined(__NR_eventfd2)
#define __NR_eventfd2 328
#endif

#if !defined(__NR_epoll_create1)
#define __NR_epoll_create1 329
#endif

#if !defined(__NR_dup3)
#define __NR_dup3 330
#endif

#if !defined(__NR_pipe2)
#define __NR_pipe2 331
#endif

#if !defined(__NR_inotify_init1)
#define __NR_inotify_init1 332
#endif

#if !defined(__NR_preadv)
#define __NR_preadv 333
#endif

#if !defined(__NR_pwritev)
#define __NR_pwritev 334
#endif

#if !defined(__NR_rt_tgsigqueueinfo)
#define __NR_rt_tgsigqueueinfo 335
#endif

#if !defined(__NR_perf_event_open)
#define __NR_perf_event_open 336
#endif

#if !defined(__NR_recvmmsg)
#define __NR_recvmmsg 337
#endif

#if !defined(__NR_fanotify_init)
#define __NR_fanotify_init 338
#endif

#if !defined(__NR_fanotify_mark)
#define __NR_fanotify_mark 339
#endif

#if !defined(__NR_prlimit64)
#define __NR_prlimit64 340
#endif

#if !defined(__NR_name_to_handle_at)
#define __NR_name_to_handle_at 341
#endif

#if !defined(__NR_open_by_handle_at)
#define __NR_open_by_handle_at 342
#endif

#if !defined(__NR_clock_adjtime)
#define __NR_clock_adjtime 343
#endif

#if !defined(__NR_syncfs)
#define __NR_syncfs 344
#endif

#if !defined(__NR_sendmmsg)
#define __NR_sendmmsg 345
#endif

#if !defined(__NR_setns)
#define __NR_setns 346
#endif

#if !defined(__NR_process_vm_readv)
#define __NR_process_vm_readv 347
#endif

#if !defined(__NR_process_vm_writev)
#define __NR_process_vm_writev 348
#endif

#if !defined(__NR_kcmp)
#define __NR_kcmp 349
#endif

#if !defined(__NR_finit_module)
#define __NR_finit_module 350
#endif

#if !defined(__NR_sched_setattr)
#define __NR_sched_setattr 351
#endif

#if !defined(__NR_sched_getattr)
#define __NR_sched_getattr 352
#endif

#if !defined(__NR_renameat2)
#define __NR_renameat2 353
#endif

#if !defined(__NR_seccomp)
#define __NR_seccomp 354
#endif

#if !defined(__NR_getrandom)
#define __NR_getrandom 355
#endif

#if !defined(__NR_memfd_create)
#define __NR_memfd_create 356
#endif

#if !defined(__NR_bpf)
#define __NR_bpf 357
#endif

#if !defined(__NR_execveat)
#define __NR_execveat 358
#endif

#if !defined(__NR_socket)
#define __NR_socket 359
#endif

#if !defined(__NR_socketpair)
#define __NR_socketpair 360
#endif

#if !defined(__NR_bind)
#define __NR_bind 361
#endif

#if !defined(__NR_connect)
#define __NR_connect 362
#endif

#if !defined(__NR_listen)
#define __NR_listen 363
#endif

#if !defined(__NR_accept4)
#define __NR_accept4 364
#endif

#if !defined(__NR_getsockopt)
#define __NR_getsockopt 365
#endif

#if !defined(__NR_setsockopt)
#define __NR_setsockopt 366
#endif

#if !defined(__NR_getsockname)
#define __NR_getsockname 367
#endif

#if !defined(__NR_getpeername)
#define __NR_getpeername 368
#endif

#if !defined(__NR_sendto)
#define __NR_sendto 369
#endif

#if !defined(__NR_sendmsg)
#define __NR_sendmsg 370
#endif

#if !defined(__NR_recvfrom)
#define __NR_recvfrom 371
#endif

#if !defined(__NR_recvmsg)
#define __NR_recvmsg 372
#endif

#if !defined(__NR_shutdown)
#define __NR_shutdown 373
#endif

#if !defined(__NR_userfaultfd)
#define __NR_userfaultfd 374
#endif

#if !defined(__NR_membarrier)
#define __NR_membarrier 375
#endif

#if !defined(__NR_mlock2)
#define __NR_mlock2 376
#endif

#if !defined(__NR_copy_file_range)
#define __NR_copy_file_range 377
#endif

#if !defined(__NR_preadv2)
#define __NR_preadv2 378
#endif

#if !defined(__NR_pwritev2)
#define __NR_pwritev2 379
#endif

#if !defined(__NR_pkey_mprotect)
#define __NR_pkey_mprotect 380
#endif

#if !defined(__NR_pkey_alloc)
#define __NR_pkey_alloc 381
#endif

#if !defined(__NR_pkey_free)
#define __NR_pkey_free 382
#endif

#if !defined(__NR_statx)
#define __NR_statx 383
#endif

#if !defined(__NR_arch_prctl)
#define __NR_arch_prctl 384
#endif

#if !defined(__NR_io_pgetevents)
#define __NR_io_pgetevents 385
#endif

#if !defined(__NR_rseq)
#define __NR_rseq 386
#endif

#if !defined(__NR_semget)
#define __NR_semget 393
#endif

#if !defined(__NR_semctl)
#define __NR_semctl 394
#endif

#if !defined(__NR_shmget)
#define __NR_shmget 395
#endif

#if !defined(__NR_shmctl)
#define __NR_shmctl 396
#endif

#if !defined(__NR_shmat)
#define __NR_shmat 397
#endif

#if !defined(__NR_shmdt)
#define __NR_shmdt 398
#endif

#if !defined(__NR_msgget)
#define __NR_msgget 399
#endif

#if !defined(__NR_msgsnd)
#define __NR_msgsnd 400
#endif

#if !defined(__NR_msgrcv)
#define __NR_msgrcv 401
#endif

#if !defined(__NR_msgctl)
#define __NR_msgctl 402
#endif

#if !defined(__NR_clock_gettime64)
#define __NR_clock_gettime64 403
#endif

#if !defined(__NR_clock_settime64)
#define __NR_clock_settime64 404
#endif

#if !defined(__NR_clock_adjtime64)
#define __NR_clock_adjtime64 405
#endif

#if !defined(__NR_clock_getres_time64)
#define __NR_clock_getres_time64 406
#endif

#if !defined(__NR_clock_nanosleep_time64)
#define __NR_clock_nanosleep_time64 407
#endif

#if !defined(__NR_timer_gettime64)
#define __NR_timer_gettime64 408
#endif

#if !defined(__NR_timer_settime64)
#define __NR_timer_settime64 409
#endif

#if !defined(__NR_timerfd_gettime64)
#define __NR_timerfd_gettime64 410
#endif

#if !defined(__NR_timerfd_settime64)
#define __NR_timerfd_settime64 411
#endif

#if !defined(__NR_utimensat_time64)
#define __NR_utimensat_time64 412
#endif

#if !defined(__NR_pselect6_time64)
#define __NR_pselect6_time64 413
#endif

#if !defined(__NR_ppoll_time64)
#define __NR_ppoll_time64 414
#endif

#if !defined(__NR_io_pgetevents_time64)
#define __NR_io_pgetevents_time64 416
#endif

#if !defined(__NR_recvmmsg_time64)
#define __NR_recvmmsg_time64 417
#endif

#if !defined(__NR_mq_timedsend_time64)
#define __NR_mq_timedsend_time64 418
#endif

#if !defined(__NR_mq_timedreceive_time64)
#define __NR_mq_timedreceive_time64 419
#endif

#if !defined(__NR_semtimedop_time64)
#define __NR_semtimedop_time64 420
#endif

#if !defined(__NR_rt_sigtimedwait_time64)
#define __NR_rt_sigtimedwait_time64 421
#endif

#if !defined(__NR_futex_time64)
#define __NR_futex_time64 422
#endif

#if !defined(__NR_sched_rr_get_interval_time64)
#define __NR_sched_rr_get_interval_time64 423
#endif

#if !defined(__NR_pidfd_send_signal)
#define __NR_pidfd_send_signal 424
#endif

#if !defined(__NR_io_uring_setup)
#define __NR_io_uring_setup 425
#endif

#if !defined(__NR_io_uring_enter)
#define __NR_io_uring_enter 426
#endif

#if !defined(__NR_io_uring_register)
#define __NR_io_uring_register 427
#endif

#if !defined(__NR_open_tree)
#define __NR_open_tree 428
#endif

#if !defined(__NR_move_mount)
#define __NR_move_mount 429
#endif

#if !defined(__NR_fsopen)
#define __NR_fsopen 430
#endif

#if !defined(__NR_fsconfig)
#define __NR_fsconfig 431
#endif

#if !defined(__NR_fsmount)
#define __NR_fsmount 432
#endif

#if !defined(__NR_fspick)
#define __NR_fspick 433
#endif

#if !defined(__NR_pidfd_open)
#define __NR_pidfd_open 434
#endif

#if !defined(__NR_clone3)
#define __NR_clone3 435
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_X86_32_LINUX_SYSCALLS_H_

