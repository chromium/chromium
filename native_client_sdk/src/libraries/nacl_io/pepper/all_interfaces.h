// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_PEPPER_ALL_INTERFACES_H_
#define LIBRARIES_NACL_IO_PEPPER_ALL_INTERFACES_H_

/* Given an interface like this:
 *
 *   struct PPB_Frob_1_1 {
 *     void (*Flange)(int32_t param1, char* param2);
 *     int32_t (*Shlep)(PP_CompletionCallback);
 *   };
 *
 * Write a set of macros like this:
 *
 *   BEGIN_INTERFACE(FrobInterface, PPB_Frob_1_1, PPB_FROB_INTERFACE_1_1)
 *     METHOD2(FrobInterface, void, Flange, int32_t, char*)
 *     METHOD1(FrobInterface, int32_t, Shlep, PP_CompletionCallback)
 *   END_INTERFACE(FrobInterface, PPB_Frob_1_1)
 *
 * NOTE:  Use versioned struct name and macro to ensure they match.
 */

/* Chrome M25 required */
BEGIN_INTERFACE(ConsoleInterface, PPB_Console_1_0, PPB_CONSOLE_INTERFACE_1_0)
  METHOD3(ConsoleInterface, void, Log, PP_Instance, PP_LogLevel, PP_Var)
END_INTERFACE(ConsoleInterface, PPB_Console_1_0)

/* Chrome M14 required */
BEGIN_INTERFACE(CoreInterface, PPB_Core_1_0, PPB_CORE_INTERFACE_1_0)
  METHOD1(CoreInterface, void, AddRefResource, PP_Resource)
  METHOD1(CoreInterface, void, ReleaseResource, PP_Resource)
  METHOD0(CoreInterface, PP_Bool, IsMainThread)
END_INTERFACE(CoreInterface, PPB_Core_1_0)

/* Chrome M14 required */
BEGIN_INTERFACE(FileIoInterface, PPB_FileIO_1_0, PPB_FILEIO_INTERFACE_1_0)
  METHOD1(FileIoInterface, void, Close, PP_Resource)
  METHOD1(FileIoInterface, PP_Resource, Create, PP_Resource)
  METHOD2(FileIoInterface, int32_t, Flush, PP_Resource,
          PP_CompletionCallback)
  METHOD4(FileIoInterface, int32_t, Open, PP_Resource, PP_Resource, int32_t,
          PP_CompletionCallback)
  METHOD3(FileIoInterface, int32_t, Query, PP_Resource, PP_FileInfo*,
          PP_CompletionCallback)
  METHOD5(FileIoInterface, int32_t, Read, PP_Resource, int64_t, char*,
          int32_t, PP_CompletionCallback)
  METHOD3(FileIoInterface, int32_t, SetLength, PP_Resource, int64_t,
          PP_CompletionCallback)
  METHOD5(FileIoInterface, int32_t, Write, PP_Resource, int64_t,
          const char*, int32_t, PP_CompletionCallback)
END_INTERFACE(FileIoInterface, PPB_FileIO_1_0)

/* Chrome M28 required */
BEGIN_INTERFACE(FileRefInterface, PPB_FileRef_1_1, PPB_FILEREF_INTERFACE_1_1)
  METHOD2(FileRefInterface, PP_Resource, Create, PP_Resource, const char*)
  METHOD2(FileRefInterface, int32_t, Delete, PP_Resource, PP_CompletionCallback)
  METHOD1(FileRefInterface, PP_Var, GetName, PP_Resource)
  METHOD3(FileRefInterface, int32_t, MakeDirectory, PP_Resource, PP_Bool,
          PP_CompletionCallback)
  METHOD3(FileRefInterface, int32_t, Query, PP_Resource, PP_FileInfo*,
          PP_CompletionCallback)
  METHOD3(FileRefInterface, int32_t, ReadDirectoryEntries, PP_Resource,
          const PP_ArrayOutput&, PP_CompletionCallback)
  METHOD3(FileRefInterface, int32_t, Rename, PP_Resource, PP_Resource,
          PP_CompletionCallback)
END_INTERFACE(FileRefInterface, PPB_FileRef_1_1)

/* Chrome M14 required */
BEGIN_INTERFACE(FileSystemInterface, PPB_FileSystem_1_0,
                PPB_FILESYSTEM_INTERFACE_1_0)
  METHOD1(FileSystemInterface, PP_Bool, IsFileSystem, PP_Resource)
  METHOD2(FileSystemInterface, PP_Resource, Create, PP_Instance,
          PP_FileSystemType)
  METHOD3(FileSystemInterface, int32_t, Open, PP_Resource, int64_t,
          PP_CompletionCallback)
END_INTERFACE(FileSystemInterface, PPB_FileSystem_1_0)

/* Chrome M25 required */
BEGIN_INTERFACE(MessageLoopInterface, PPB_MessageLoop_1_0,
                PPB_MESSAGELOOP_INTERFACE_1_0)
  METHOD1(MessageLoopInterface, PP_Resource, Create, PP_Instance)
  METHOD1(MessageLoopInterface, int32_t, AttachToCurrentThread, PP_Resource)
  METHOD1(MessageLoopInterface, int32_t, Run, PP_Resource)
  METHOD3(MessageLoopInterface, int32_t, PostWork, PP_Resource,
          struct PP_CompletionCallback, int64_t)
  METHOD2(MessageLoopInterface, int32_t, PostQuit, PP_Resource, PP_Bool)
  METHOD0(MessageLoopInterface, PP_Resource, GetCurrent)
  METHOD0(MessageLoopInterface, PP_Resource, GetForMainThread)
END_INTERFACE(MessageLoopInterface, PPB_MessageLoop_1_0)

/* Chrome M14 required */
BEGIN_INTERFACE(MessagingInterface, PPB_Messaging_1_0,
                PPB_MESSAGING_INTERFACE_1_0)
  METHOD2(MessagingInterface, void, PostMessage, PP_Instance, PP_Var)
END_INTERFACE(MessagingInterface, PPB_Messaging_1_0)

/* Chrome M29 required */
BEGIN_INTERFACE(VarArrayInterface, PPB_VarArray_1_0,
                PPB_VAR_ARRAY_INTERFACE_1_0)
  METHOD0(VarArrayInterface, PP_Var, Create)
  METHOD2(VarArrayInterface, PP_Var, Get, PP_Var, uint32_t)
  METHOD3(VarArrayInterface, PP_Bool, Set, PP_Var, uint32_t, PP_Var)
  METHOD1(VarArrayInterface, uint32_t, GetLength, PP_Var)
  METHOD2(VarArrayInterface, PP_Bool, SetLength, PP_Var, uint32_t)
END_INTERFACE(VarArrayInterface, PPB_VarArray_1_0)

/* Chrome M18 required */
BEGIN_INTERFACE(VarArrayBufferInterface, PPB_VarArrayBuffer_1_0,
                PPB_VAR_ARRAY_BUFFER_INTERFACE_1_0)
  METHOD1(VarArrayBufferInterface, PP_Var, Create, uint32_t)
  METHOD2(VarArrayBufferInterface, PP_Bool, ByteLength, PP_Var, uint32_t*)
  METHOD1(VarArrayBufferInterface, void*, Map, PP_Var)
  METHOD1(VarArrayBufferInterface, void, Unmap, PP_Var)
END_INTERFACE(VarArrayBufferInterface, PPB_VarArrayBuffer_1_0)

BEGIN_INTERFACE(VarDictionaryInterface, PPB_VarDictionary_1_0,
                PPB_VAR_DICTIONARY_INTERFACE_1_0)
  METHOD0(VarDictionaryInterface, PP_Var, Create)
  METHOD3(VarDictionaryInterface, PP_Bool, Set, PP_Var, PP_Var, PP_Var)
  METHOD2(VarDictionaryInterface, PP_Var, Get, PP_Var, PP_Var)
  METHOD1(VarDictionaryInterface, PP_Var, GetKeys, PP_Var)
END_INTERFACE(VarDictionaryInterface, PPB_VarDictionary_1_0)

/* Chrome M18 required */
BEGIN_INTERFACE(VarInterface, PPB_Var_1_1, PPB_VAR_INTERFACE_1_1)
  METHOD1(VarInterface, void, AddRef, PP_Var)
  METHOD1(VarInterface, void, Release, PP_Var)
  METHOD2(VarInterface, PP_Var, VarFromUtf8, const char *, uint32_t)
  METHOD2(VarInterface, const char*, VarToUtf8, PP_Var, uint32_t*)
END_INTERFACE(VarInterface, PPB_Var_1_1)

/* Chrome M29 required */
BEGIN_INTERFACE(HostResolverInterface, PPB_HostResolver_1_0,
                PPB_HOSTRESOLVER_INTERFACE_1_0)
  METHOD1(HostResolverInterface, PP_Resource, Create, PP_Instance)
  METHOD5(HostResolverInterface, int32_t, Resolve, PP_Resource, const char*,
          uint16_t, const struct PP_HostResolver_Hint*,
          struct PP_CompletionCallback)
  METHOD1(HostResolverInterface, PP_Var, GetCanonicalName, PP_Resource)
  METHOD1(HostResolverInterface, uint32_t, GetNetAddressCount, PP_Resource)
  METHOD2(HostResolverInterface, PP_Resource, GetNetAddress,
          PP_Resource, uint32_t)
END_INTERFACE(HostResolverInterface, PPB_HostResolver_1_0)

/* Chrome M29 required */
BEGIN_INTERFACE(NetAddressInterface, PPB_NetAddress_1_0,
                PPB_NETADDRESS_INTERFACE_1_0)
  METHOD2(NetAddressInterface, PP_Resource, CreateFromIPv4Address,
          PP_Instance, PP_NetAddress_IPv4*)
  METHOD2(NetAddressInterface, PP_Resource, CreateFromIPv6Address,
          PP_Instance, PP_NetAddress_IPv6*)
  METHOD1(NetAddressInterface, PP_Bool, IsNetAddress, PP_Resource)
  METHOD1(NetAddressInterface, PP_NetAddress_Family, GetFamily, PP_Resource)
  METHOD2(NetAddressInterface, PP_Bool, DescribeAsIPv4Address, PP_Resource,
          struct PP_NetAddress_IPv4*)
  METHOD2(NetAddressInterface, PP_Bool, DescribeAsIPv6Address, PP_Resource,
          struct PP_NetAddress_IPv6*)
  METHOD2(NetAddressInterface, PP_Var, DescribeAsString, PP_Resource, PP_Bool)
END_INTERFACE(NetAddressInterface, PPB_NetAddress_1_0)

/* Chrome M14 required */
BEGIN_INTERFACE(URLLoaderInterface, PPB_URLLoader_1_0,
                PPB_URLLOADER_INTERFACE_1_0)
  METHOD1(URLLoaderInterface, PP_Resource, Create, PP_Instance)
  METHOD3(URLLoaderInterface, int32_t, Open, PP_Resource, PP_Resource,
          PP_CompletionCallback)
  METHOD1(URLLoaderInterface, PP_Resource, GetResponseInfo, PP_Resource)
  METHOD4(URLLoaderInterface, int32_t, ReadResponseBody, PP_Resource, void*,
          int32_t, PP_CompletionCallback)
  METHOD2(URLLoaderInterface, int32_t, FinishStreamingToFile, PP_Resource,
          PP_CompletionCallback)
  METHOD1(URLLoaderInterface, void, Close, PP_Resource)
END_INTERFACE(URLLoaderInterface, PPB_URLLoader_1_0)

/* Chrome M14 required */
BEGIN_INTERFACE(URLRequestInfoInterface, PPB_URLRequestInfo_1_0,
                PPB_URLREQUESTINFO_INTERFACE_1_0)
  METHOD1(URLRequestInfoInterface, PP_Resource, Create, PP_Instance)
  METHOD3(URLRequestInfoInterface, PP_Bool, SetProperty, PP_Resource,
          PP_URLRequestProperty, PP_Var)
  METHOD3(URLRequestInfoInterface, PP_Bool, AppendDataToBody, PP_Resource,
          const void*, uint32_t)
END_INTERFACE(URLRequestInfoInterface, PPB_URLRequestInfo_1_0)

/* Chrome M14 required */
BEGIN_INTERFACE(URLResponseInfoInterface, PPB_URLResponseInfo_1_0,
                PPB_URLRESPONSEINFO_INTERFACE_1_0)
  METHOD2(URLResponseInfoInterface, PP_Var, GetProperty, PP_Resource,
          PP_URLResponseProperty)
  METHOD1(URLResponseInfoInterface, PP_Resource, GetBodyAsFileRef,
          PP_Resource)
END_INTERFACE(URLResponseInfoInterface, PPB_URLResponseInfo_1_0)

/* Chrome M31 required */
BEGIN_INTERFACE(TCPSocketInterface, PPB_TCPSocket_1_1,
                PPB_TCPSOCKET_INTERFACE_1_1)
  METHOD1(TCPSocketInterface, PP_Resource, Create, PP_Instance)
  METHOD3(TCPSocketInterface, int32_t, Bind, PP_Resource, PP_Resource,
          PP_CompletionCallback)
  METHOD3(TCPSocketInterface, int32_t, Listen, PP_Resource, int32_t,
          PP_CompletionCallback)
  METHOD3(TCPSocketInterface, int32_t, Accept, PP_Resource, PP_Resource*,
          PP_CompletionCallback)
  METHOD1(TCPSocketInterface, PP_Bool, IsTCPSocket, PP_Resource)
  METHOD3(TCPSocketInterface, int32_t, Connect, PP_Resource, PP_Resource,
          PP_CompletionCallback)
  METHOD1(TCPSocketInterface, PP_Resource, GetLocalAddress, PP_Resource)
  METHOD1(TCPSocketInterface, PP_Resource, GetRemoteAddress, PP_Resource)
  METHOD4(TCPSocketInterface, int32_t, Read, PP_Resource, char*, int32_t,
          PP_CompletionCallback)
  METHOD4(TCPSocketInterface, int32_t, Write, PP_Resource, const char*,
          int32_t, PP_CompletionCallback)
  METHOD1(TCPSocketInterface, void, Close, PP_Resource)
  METHOD4(TCPSocketInterface, int32_t, SetOption, PP_Resource,
          PP_TCPSocket_Option, PP_Var, PP_CompletionCallback)
END_INTERFACE(TCPSocketInterface, PPB_TCPSocket_1_1)

/* Chrome M44 required */
BEGIN_INTERFACE(UDPSocketInterface,
                PPB_UDPSocket_1_2,
                PPB_UDPSOCKET_INTERFACE_1_2)
  METHOD1(UDPSocketInterface, PP_Resource, Create, PP_Instance)
  METHOD1(UDPSocketInterface, PP_Bool, IsUDPSocket, PP_Resource)
  METHOD3(UDPSocketInterface, int32_t, Bind, PP_Resource, PP_Resource,
          PP_CompletionCallback)
  METHOD1(UDPSocketInterface, PP_Resource, GetBoundAddress, PP_Resource)
  METHOD5(UDPSocketInterface, int32_t, RecvFrom, PP_Resource, char*, int32_t,
          PP_Resource*, PP_CompletionCallback)
  METHOD5(UDPSocketInterface, int32_t, SendTo, PP_Resource, const char*,
          int32_t, PP_Resource, PP_CompletionCallback)
  METHOD1(UDPSocketInterface, void, Close, PP_Resource)
  METHOD4(UDPSocketInterface, int32_t, SetOption, PP_Resource,
          PP_UDPSocket_Option, PP_Var, PP_CompletionCallback)
  METHOD3(UDPSocketInterface,
          int32_t,
          JoinGroup,
          PP_Resource,
          PP_Resource,
          PP_CompletionCallback)
  METHOD3(UDPSocketInterface,
          int32_t,
          LeaveGroup,
          PP_Resource,
          PP_Resource,
          PP_CompletionCallback)
  END_INTERFACE(UDPSocketInterface, PPB_UDPSocket_1_2)

#endif  // LIBRARIES_NACL_IO_PEPPER_ALL_INTERFACES_H_