// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_ARM64_LINUX_SYSCALLS_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_ARM64_LINUX_SYSCALLS_H_

#include <asm-generic/unistd.h>

#if !defined(__NR_io_setup)
#define __NR_io_setup 0
#endif

#if !defined(__NR_io_destroy)
#define __NR_io_destroy 1
#endif

#if !defined(__NR_io_submit)
#define __NR_io_submit 2
#endif

#if !defined(__NR_io_cancel)
#define __NR_io_cancel 3
#endif

#if !defined(__NR_io_getevents)
#define __NR_io_getevents 4
#endif

#if !defined(__NR_setxattr)
#define __NR_setxattr 5
#endif

#if !defined(__NR_lsetxattr)
#define __NR_lsetxattr 6
#endif

#if !defined(__NR_fsetxattr)
#define __NR_fsetxattr 7
#endif

#if !defined(__NR_getxattr)
#define __NR_getxattr 8
#endif

#if !defined(__NR_lgetxattr)
#define __NR_lgetxattr 9
#endif

#if !defined(__NR_fgetxattr)
#define __NR_fgetxattr 10
#endif

#if !defined(__NR_listxattr)
#define __NR_listxattr 11
#endif

#if !defined(__NR_llistxattr)
#define __NR_llistxattr 12
#endif

#if !defined(__NR_flistxattr)
#define __NR_flistxattr 13
#endif

#if !defined(__NR_removexattr)
#define __NR_removexattr 14
#endif

#if !defined(__NR_lremovexattr)
#define __NR_lremovexattr 15
#endif

#if !defined(__NR_fremovexattr)
#define __NR_fremovexattr 16
#endif

#if !defined(__NR_getcwd)
#define __NR_getcwd 17
#endif

#if !defined(__NR_lookup_dcookie)
#define __NR_lookup_dcookie 18
#endif

#if !defined(__NR_eventfd2)
#define __NR_eventfd2 19
#endif

#if !defined(__NR_epoll_create1)
#define __NR_epoll_create1 20
#endif

#if !defined(__NR_epoll_ctl)
#define __NR_epoll_ctl 21
#endif

#if !defined(__NR_epoll_pwait)
#define __NR_epoll_pwait 22
#endif

#if !defined(__NR_dup)
#define __NR_dup 23
#endif

#if !defined(__NR_dup3)
#define __NR_dup3 24
#endif

#if !defined(__NR_fcntl)
#define __NR_fcntl 25
#endif

#if !defined(__NR_inotify_init1)
#define __NR_inotify_init1 26
#endif

#if !defined(__NR_inotify_add_watch)
#define __NR_inotify_add_watch 27
#endif

#if !defined(__NR_inotify_rm_watch)
#define __NR_inotify_rm_watch 28
#endif

#if !defined(__NR_ioctl)
#define __NR_ioctl 29
#endif

#if !defined(__NR_ioprio_set)
#define __NR_ioprio_set 30
#endif

#if !defined(__NR_ioprio_get)
#define __NR_ioprio_get 31
#endif

#if !defined(__NR_flock)
#define __NR_flock 32
#endif

#if !defined(__NR_mknodat)
#define __NR_mknodat 33
#endif

#if !defined(__NR_mkdirat)
#define __NR_mkdirat 34
#endif

#if !defined(__NR_unlinkat)
#define __NR_unlinkat 35
#endif

#if !defined(__NR_symlinkat)
#define __NR_symlinkat 36
#endif

#if !defined(__NR_linkat)
#define __NR_linkat 37
#endif

#if !defined(__NR_renameat)
#define __NR_renameat 38
#endif

#if !defined(__NR_umount2)
#define __NR_umount2 39
#endif

#if !defined(__NR_mount)
#define __NR_mount 40
#endif

#if !defined(__NR_pivot_root)
#define __NR_pivot_root 41
#endif

#if !defined(__NR_nfsservctl)
#define __NR_nfsservctl 42
#endif

#if !defined(__NR_statfs)
#define __NR_statfs 43
#endif

#if !defined(__NR_fstatfs)
#define __NR_fstatfs 44
#endif

#if !defined(__NR_truncate)
#define __NR_truncate 45
#endif

#if !defined(__NR_ftruncate)
#define __NR_ftruncate 46
#endif

#if !defined(__NR_fallocate)
#define __NR_fallocate 47
#endif

#if !defined(__NR_faccessat)
#define __NR_faccessat 48
#endif

#if !defined(__NR_chdir)
#define __NR_chdir 49
#endif

#if !defined(__NR_fchdir)
#define __NR_fchdir 50
#endif

#if !defined(__NR_chroot)
#define __NR_chroot 51
#endif

#if !defined(__NR_fchmod)
#define __NR_fchmod 52
#endif

#if !defined(__NR_fchmodat)
#define __NR_fchmodat 53
#endif

#if !defined(__NR_fchownat)
#define __NR_fchownat 54
#endif

#if !defined(__NR_fchown)
#define __NR_fchown 55
#endif

#if !defined(__NR_openat)
#define __NR_openat 56
#endif

#if !defined(__NR_close)
#define __NR_close 57
#endif

#if !defined(__NR_vhangup)
#define __NR_vhangup 58
#endif

#if !defined(__NR_pipe2)
#define __NR_pipe2 59
#endif

#if !defined(__NR_quotactl)
#define __NR_quotactl 60
#endif

#if !defined(__NR_getdents64)
#define __NR_getdents64 61
#endif

#if !defined(__NR_lseek)
#define __NR_lseek 62
#endif

#if !defined(__NR_read)
#define __NR_read 63
#endif

#if !defined(__NR_write)
#define __NR_write 64
#endif

#if !defined(__NR_readv)
#define __NR_readv 65
#endif

#if !defined(__NR_writev)
#define __NR_writev 66
#endif

#if !defined(__NR_pread64)
#define __NR_pread64 67
#endif

#if !defined(__NR_pwrite64)
#define __NR_pwrite64 68
#endif

#if !defined(__NR_preadv)
#define __NR_preadv 69
#endif

#if !defined(__NR_pwritev)
#define __NR_pwritev 70
#endif

#if !defined(__NR_sendfile)
#define __NR_sendfile 71
#endif

#if !defined(__NR_pselect6)
#define __NR_pselect6 72
#endif

#if !defined(__NR_ppoll)
#define __NR_ppoll 73
#endif

#if !defined(__NR_signalfd4)
#define __NR_signalfd4 74
#endif

#if !defined(__NR_vmsplice)
#define __NR_vmsplice 75
#endif

#if !defined(__NR_splice)
#define __NR_splice 76
#endif

#if !defined(__NR_tee)
#define __NR_tee 77
#endif

#if !defined(__NR_readlinkat)
#define __NR_readlinkat 78
#endif

#if !defined(__NR_newfstatat)
#define __NR_newfstatat 79
#endif

#if !defined(__NR_fstat)
#define __NR_fstat 80
#endif

#if !defined(__NR_sync)
#define __NR_sync 81
#endif

#if !defined(__NR_fsync)
#define __NR_fsync 82
#endif

#if !defined(__NR_fdatasync)
#define __NR_fdatasync 83
#endif

#if !defined(__NR_sync_file_range)
#define __NR_sync_file_range 84
#endif

#if !defined(__NR_timerfd_create)
#define __NR_timerfd_create 85
#endif

#if !defined(__NR_timerfd_settime)
#define __NR_timerfd_settime 86
#endif

#if !defined(__NR_timerfd_gettime)
#define __NR_timerfd_gettime 87
#endif

#if !defined(__NR_utimensat)
#define __NR_utimensat 88
#endif

#if !defined(__NR_acct)
#define __NR_acct 89
#endif

#if !defined(__NR_capget)
#define __NR_capget 90
#endif

#if !defined(__NR_capset)
#define __NR_capset 91
#endif

#if !defined(__NR_personality)
#define __NR_personality 92
#endif

#if !defined(__NR_exit)
#define __NR_exit 93
#endif

#if !defined(__NR_exit_group)
#define __NR_exit_group 94
#endif

#if !defined(__NR_waitid)
#define __NR_waitid 95
#endif

#if !defined(__NR_set_tid_address)
#define __NR_set_tid_address 96
#endif

#if !defined(__NR_unshare)
#define __NR_unshare 97
#endif

#if !defined(__NR_futex)
#define __NR_futex 98
#endif

#if !defined(__NR_set_robust_list)
#define __NR_set_robust_list 99
#endif

#if !defined(__NR_get_robust_list)
#define __NR_get_robust_list 100
#endif

#if !defined(__NR_nanosleep)
#define __NR_nanosleep 101
#endif

#if !defined(__NR_getitimer)
#define __NR_getitimer 102
#endif

#if !defined(__NR_setitimer)
#define __NR_setitimer 103
#endif

#if !defined(__NR_kexec_load)
#define __NR_kexec_load 104
#endif

#if !defined(__NR_init_module)
#define __NR_init_module 105
#endif

#if !defined(__NR_delete_module)
#define __NR_delete_module 106
#endif

#if !defined(__NR_timer_create)
#define __NR_timer_create 107
#endif

#if !defined(__NR_timer_gettime)
#define __NR_timer_gettime 108
#endif

#if !defined(__NR_timer_getoverrun)
#define __NR_timer_getoverrun 109
#endif

#if !defined(__NR_timer_settime)
#define __NR_timer_settime 110
#endif

#if !defined(__NR_timer_delete)
#define __NR_timer_delete 111
#endif

#if !defined(__NR_clock_settime)
#define __NR_clock_settime 112
#endif

#if !defined(__NR_clock_gettime)
#define __NR_clock_gettime 113
#endif

#if !defined(__NR_clock_getres)
#define __NR_clock_getres 114
#endif

#if !defined(__NR_clock_nanosleep)
#define __NR_clock_nanosleep 115
#endif

#if !defined(__NR_syslog)
#define __NR_syslog 116
#endif

#if !defined(__NR_ptrace)
#define __NR_ptrace 117
#endif

#if !defined(__NR_sched_setparam)
#define __NR_sched_setparam 118
#endif

#if !defined(__NR_sched_setscheduler)
#define __NR_sched_setscheduler 119
#endif

#if !defined(__NR_sched_getscheduler)
#define __NR_sched_getscheduler 120
#endif

#if !defined(__NR_sched_getparam)
#define __NR_sched_getparam 121
#endif

#if !defined(__NR_sched_setaffinity)
#define __NR_sched_setaffinity 122
#endif

#if !defined(__NR_sched_getaffinity)
#define __NR_sched_getaffinity 123
#endif

#if !defined(__NR_sched_yield)
#define __NR_sched_yield 124
#endif

#if !defined(__NR_sched_get_priority_max)
#define __NR_sched_get_priority_max 125
#endif

#if !defined(__NR_sched_get_priority_min)
#define __NR_sched_get_priority_min 126
#endif

#if !defined(__NR_sched_rr_get_interval)
#define __NR_sched_rr_get_interval 127
#endif

#if !defined(__NR_restart_syscall)
#define __NR_restart_syscall 128
#endif

#if !defined(__NR_kill)
#define __NR_kill 129
#endif

#if !defined(__NR_tkill)
#define __NR_tkill 130
#endif

#if !defined(__NR_tgkill)
#define __NR_tgkill 131
#endif

#if !defined(__NR_sigaltstack)
#define __NR_sigaltstack 132
#endif

#if !defined(__NR_rt_sigsuspend)
#define __NR_rt_sigsuspend 133
#endif

#if !defined(__NR_rt_sigaction)
#define __NR_rt_sigaction 134
#endif

#if !defined(__NR_rt_sigprocmask)
#define __NR_rt_sigprocmask 135
#endif

#if !defined(__NR_rt_sigpending)
#define __NR_rt_sigpending 136
#endif

#if !defined(__NR_rt_sigtimedwait)
#define __NR_rt_sigtimedwait 137
#endif

#if !defined(__NR_rt_sigqueueinfo)
#define __NR_rt_sigqueueinfo 138
#endif

#if !defined(__NR_rt_sigreturn)
#define __NR_rt_sigreturn 139
#endif

#if !defined(__NR_setpriority)
#define __NR_setpriority 140
#endif

#if !defined(__NR_getpriority)
#define __NR_getpriority 141
#endif

#if !defined(__NR_reboot)
#define __NR_reboot 142
#endif

#if !defined(__NR_setregid)
#define __NR_setregid 143
#endif

#if !defined(__NR_setgid)
#define __NR_setgid 144
#endif

#if !defined(__NR_setreuid)
#define __NR_setreuid 145
#endif

#if !defined(__NR_setuid)
#define __NR_setuid 146
#endif

#if !defined(__NR_setresuid)
#define __NR_setresuid 147
#endif

#if !defined(__NR_getresuid)
#define __NR_getresuid 148
#endif

#if !defined(__NR_setresgid)
#define __NR_setresgid 149
#endif

#if !defined(__NR_getresgid)
#define __NR_getresgid 150
#endif

#if !defined(__NR_setfsuid)
#define __NR_setfsuid 151
#endif

#if !defined(__NR_setfsgid)
#define __NR_setfsgid 152
#endif

#if !defined(__NR_times)
#define __NR_times 153
#endif

#if !defined(__NR_setpgid)
#define __NR_setpgid 154
#endif

#if !defined(__NR_getpgid)
#define __NR_getpgid 155
#endif

#if !defined(__NR_getsid)
#define __NR_getsid 156
#endif

#if !defined(__NR_setsid)
#define __NR_setsid 157
#endif

#if !defined(__NR_getgroups)
#define __NR_getgroups 158
#endif

#if !defined(__NR_setgroups)
#define __NR_setgroups 159
#endif

#if !defined(__NR_uname)
#define __NR_uname 160
#endif

#if !defined(__NR_sethostname)
#define __NR_sethostname 161
#endif

#if !defined(__NR_setdomainname)
#define __NR_setdomainname 162
#endif

#if !defined(__NR_getrlimit)
#define __NR_getrlimit 163
#endif

#if !defined(__NR_setrlimit)
#define __NR_setrlimit 164
#endif

#if !defined(__NR_getrusage)
#define __NR_getrusage 165
#endif

#if !defined(__NR_umask)
#define __NR_umask 166
#endif

#if !defined(__NR_prctl)
#define __NR_prctl 167
#endif

#if !defined(__NR_getcpu)
#define __NR_getcpu 168
#endif

#if !defined(__NR_gettimeofday)
#define __NR_gettimeofday 169
#endif

#if !defined(__NR_settimeofday)
#define __NR_settimeofday 170
#endif

#if !defined(__NR_adjtimex)
#define __NR_adjtimex 171
#endif

#if !defined(__NR_getpid)
#define __NR_getpid 172
#endif

#if !defined(__NR_getppid)
#define __NR_getppid 173
#endif

#if !defined(__NR_getuid)
#define __NR_getuid 174
#endif

#if !defined(__NR_geteuid)
#define __NR_geteuid 175
#endif

#if !defined(__NR_getgid)
#define __NR_getgid 176
#endif

#if !defined(__NR_getegid)
#define __NR_getegid 177
#endif

#if !defined(__NR_gettid)
#define __NR_gettid 178
#endif

#if !defined(__NR_sysinfo)
#define __NR_sysinfo 179
#endif

#if !defined(__NR_mq_open)
#define __NR_mq_open 180
#endif

#if !defined(__NR_mq_unlink)
#define __NR_mq_unlink 181
#endif

#if !defined(__NR_mq_timedsend)
#define __NR_mq_timedsend 182
#endif

#if !defined(__NR_mq_timedreceive)
#define __NR_mq_timedreceive 183
#endif

#if !defined(__NR_mq_notify)
#define __NR_mq_notify 184
#endif

#if !defined(__NR_mq_getsetattr)
#define __NR_mq_getsetattr 185
#endif

#if !defined(__NR_msgget)
#define __NR_msgget 186
#endif

#if !defined(__NR_msgctl)
#define __NR_msgctl 187
#endif

#if !defined(__NR_msgrcv)
#define __NR_msgrcv 188
#endif

#if !defined(__NR_msgsnd)
#define __NR_msgsnd 189
#endif

#if !defined(__NR_semget)
#define __NR_semget 190
#endif

#if !defined(__NR_semctl)
#define __NR_semctl 191
#endif

#if !defined(__NR_semtimedop)
#define __NR_semtimedop 192
#endif

#if !defined(__NR_semop)
#define __NR_semop 193
#endif

#if !defined(__NR_shmget)
#define __NR_shmget 194
#endif

#if !defined(__NR_shmctl)
#define __NR_shmctl 195
#endif

#if !defined(__NR_shmat)
#define __NR_shmat 196
#endif

#if !defined(__NR_shmdt)
#define __NR_shmdt 197
#endif

#if !defined(__NR_socket)
#define __NR_socket 198
#endif

#if !defined(__NR_socketpair)
#define __NR_socketpair 199
#endif

#if !defined(__NR_bind)
#define __NR_bind 200
#endif

#if !defined(__NR_listen)
#define __NR_listen 201
#endif

#if !defined(__NR_accept)
#define __NR_accept 202
#endif

#if !defined(__NR_connect)
#define __NR_connect 203
#endif

#if !defined(__NR_getsockname)
#define __NR_getsockname 204
#endif

#if !defined(__NR_getpeername)
#define __NR_getpeername 205
#endif

#if !defined(__NR_sendto)
#define __NR_sendto 206
#endif

#if !defined(__NR_recvfrom)
#define __NR_recvfrom 207
#endif

#if !defined(__NR_setsockopt)
#define __NR_setsockopt 208
#endif

#if !defined(__NR_getsockopt)
#define __NR_getsockopt 209
#endif

#if !defined(__NR_shutdown)
#define __NR_shutdown 210
#endif

#if !defined(__NR_sendmsg)
#define __NR_sendmsg 211
#endif

#if !defined(__NR_recvmsg)
#define __NR_recvmsg 212
#endif

#if !defined(__NR_readahead)
#define __NR_readahead 213
#endif

#if !defined(__NR_brk)
#define __NR_brk 214
#endif

#if !defined(__NR_munmap)
#define __NR_munmap 215
#endif

#if !defined(__NR_mremap)
#define __NR_mremap 216
#endif

#if !defined(__NR_add_key)
#define __NR_add_key 217
#endif

#if !defined(__NR_request_key)
#define __NR_request_key 218
#endif

#if !defined(__NR_keyctl)
#define __NR_keyctl 219
#endif

#if !defined(__NR_clone)
#define __NR_clone 220
#endif

#if !defined(__NR_execve)
#define __NR_execve 221
#endif

#if !defined(__NR_mmap)
#define __NR_mmap 222
#endif

#if !defined(__NR_fadvise64)
#define __NR_fadvise64 223
#endif

#if !defined(__NR_swapon)
#define __NR_swapon 224
#endif

#if !defined(__NR_swapoff)
#define __NR_swapoff 225
#endif

#if !defined(__NR_mprotect)
#define __NR_mprotect 226
#endif

#if !defined(__NR_msync)
#define __NR_msync 227
#endif

#if !defined(__NR_mlock)
#define __NR_mlock 228
#endif

#if !defined(__NR_munlock)
#define __NR_munlock 229
#endif

#if !defined(__NR_mlockall)
#define __NR_mlockall 230
#endif

#if !defined(__NR_munlockall)
#define __NR_munlockall 231
#endif

#if !defined(__NR_mincore)
#define __NR_mincore 232
#endif

#if !defined(__NR_madvise)
#define __NR_madvise 233
#endif

#if !defined(__NR_remap_file_pages)
#define __NR_remap_file_pages 234
#endif

#if !defined(__NR_mbind)
#define __NR_mbind 235
#endif

#if !defined(__NR_get_mempolicy)
#define __NR_get_mempolicy 236
#endif

#if !defined(__NR_set_mempolicy)
#define __NR_set_mempolicy 237
#endif

#if !defined(__NR_migrate_pages)
#define __NR_migrate_pages 238
#endif

#if !defined(__NR_move_pages)
#define __NR_move_pages 239
#endif

#if !defined(__NR_rt_tgsigqueueinfo)
#define __NR_rt_tgsigqueueinfo 240
#endif

#if !defined(__NR_perf_event_open)
#define __NR_perf_event_open 241
#endif

#if !defined(__NR_accept4)
#define __NR_accept4 242
#endif

#if !defined(__NR_recvmmsg)
#define __NR_recvmmsg 243
#endif

#if !defined(__NR_wait4)
#define __NR_wait4 260
#endif

#if !defined(__NR_prlimit64)
#define __NR_prlimit64 261
#endif

#if !defined(__NR_fanotify_init)
#define __NR_fanotify_init 262
#endif

#if !defined(__NR_fanotify_mark)
#define __NR_fanotify_mark 263
#endif

#if !defined(__NR_name_to_handle_at)
#define __NR_name_to_handle_at 264
#endif

#if !defined(__NR_open_by_handle_at)
#define __NR_open_by_handle_at 265
#endif

#if !defined(__NR_clock_adjtime)
#define __NR_clock_adjtime 266
#endif

#if !defined(__NR_syncfs)
#define __NR_syncfs 267
#endif

#if !defined(__NR_setns)
#define __NR_setns 268
#endif

#if !defined(__NR_sendmmsg)
#define __NR_sendmmsg 269
#endif

#if !defined(__NR_process_vm_readv)
#define __NR_process_vm_readv 270
#endif

#if !defined(__NR_process_vm_writev)
#define __NR_process_vm_writev 271
#endif

#if !defined(__NR_kcmp)
#define __NR_kcmp 272
#endif

#if !defined(__NR_finit_module)
#define __NR_finit_module 273
#endif

#if !defined(__NR_sched_setattr)
#define __NR_sched_setattr 274
#endif

#if !defined(__NR_sched_getattr)
#define __NR_sched_getattr 275
#endif

#if !defined(__NR_renameat2)
#define __NR_renameat2 276
#endif

#if !defined(__NR_seccomp)
#define __NR_seccomp 277
#endif

#if !defined(__NR_getrandom)
#define __NR_getrandom 278
#endif

#if !defined(__NR_memfd_create)
#define __NR_memfd_create 279
#endif

#if !defined(__NR_bpf)
#define __NR_bpf 280
#endif

#if !defined(__NR_execveat)
#define __NR_execveat 281
#endif

#if !defined(__NR_userfaultfd)
#define __NR_userfaultfd 282
#endif

#if !defined(__NR_membarrier)
#define __NR_membarrier 283
#endif

#if !defined(__NR_mlock2)
#define __NR_mlock2 284
#endif

#if !defined(__NR_copy_file_range)
#define __NR_copy_file_range 285
#endif

#if !defined(__NR_preadv2)
#define __NR_preadv2 286
#endif

#if !defined(__NR_pwritev2)
#define __NR_pwritev2 287
#endif

#if !defined(__NR_pkey_mprotect)
#define __NR_pkey_mprotect 288
#endif

#if !defined(__NR_pkey_alloc)
#define __NR_pkey_alloc 289
#endif

#if !defined(__NR_pkey_free)
#define __NR_pkey_free 290
#endif

#if !defined(__NR_statx)
#define __NR_statx 291
#endif

#if !defined(__NR_io_pgetevents)
#define __NR_io_pgetevents 292
#endif

#if !defined(__NR_rseq)
#define __NR_rseq 293
#endif

#if !defined(__NR_kexec_file_load)
#define __NR_kexec_file_load 294
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

#if !defined(__NR_close_range)
#define __NR_close_range 436
#endif

#if !defined(__NR_openat2)
#define __NR_openat2 437
#endif

#if !defined(__NR_pidfd_getfd)
#define __NR_pidfd_getfd 438
#endif

#if !defined(__NR_faccessat2)
#define __NR_faccessat2 439
#endif

#if !defined(__NR_process_madvise)
#define __NR_process_madvise 440
#endif

#if !defined(__NR_epoll_pwait2)
#define __NR_epoll_pwait2 441
#endif

#if !defined(__NR_mount_setattr)
#define __NR_mount_setattr 442
#endif

#if !defined(__NR_quotactl_path)
#define __NR_quotactl_path 443
#endif

#if !defined(__NR_landlock_create_ruleset)
#define __NR_landlock_create_ruleset 444
#endif

#if !defined(__NR_landlock_add_rule)
#define __NR_landlock_add_rule 445
#endif

#if !defined(__NR_landlock_restrict_self)
#define __NR_landlock_restrict_self 446
#endif

#if !defined(__NR_memfd_secret)
#define __NR_memfd_secret 447
#endif

#if !defined(__NR_process_mrelease)
#define __NR_process_mrelease 448
#endif

#if !defined(__NR_futex_waitv)
#define __NR_futex_waitv 449
#endif

#if !defined(__NR_set_mempolicy_home_node)
#define __NR_set_mempolicy_home_node 450
#endif

#if !defined(__NR_cachestat)
#define __NR_cachestat 451
#endif

#if !defined(__NR_fchmodat2)
#define __NR_fchmodat2 452
#endif

#if !defined(__NR_map_shadow_stack)
#define __NR_map_shadow_stack 453
#endif

#if !defined(__NR_futex_wake)
#define __NR_futex_wake 454
#endif

#if !defined(__NR_futex_wait)
#define __NR_futex_wait 455
#endif

#if !defined(__NR_futex_requeue)
#define __NR_futex_requeue 456
#endif

#if !defined(__NR_statmount)
#define __NR_statmount 457
#endif

#if !defined(__NR_listmount)
#define __NR_listmount 458
#endif

#if !defined(__NR_lsm_get_self_attr)
#define __NR_lsm_get_self_attr 459
#endif

#if !defined(__NR_lsm_set_self_attr)
#define __NR_lsm_set_self_attr 460
#endif

#if !defined(__NR_lsm_list_modules)
#define __NR_lsm_list_modules 461
#endif

#if !defined(__NR_mseal)
#define __NR_mseal 462
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_ARM64_LINUX_SYSCALLS_H_
