# Dealing with stack traces that involve inlined code

[Rendered](https://chromium.googlesource.com/chromium/src/+/main/docs/inlined_stack_traces.md)

Sometimes we get crashes with stack traces that are hard to use.
They may contain the file and line-number for the inlined method
but only the method name for the caller.
There may also be layers of the stack
that are missing completely.

E.g.

```
0xd3c90a78(libmonochrome.so -vector.h:1047 ) blink::EventDispatcher::Dispatch()
0xde86db5a
0xd3c8fcdd(libmonochrome.so -event_dispatcher.cc:59 )blink::EventDispatcher::DispatchEvent(blink::Node&, blink::Event*)
0xd3e27bb9(libmonochrome.so -container_node.cc:1279 )blink::ContainerNode::DidInsertNodeVector(blink::HeapVector<blink::Member<blink::Node>, 11u> const&, blink::Node*, blink::HeapVector<blink::Member<blink::Node>, 11u> const&)
0xd3e27485(libmonochrome.so -container_node.cc:851 )blink::ContainerNode::AppendChild(blink::Node*, blink::ExceptionState&)
0xd3eedf4f(libmonochrome.so -v8_node.cc:534 )blink::V8Node::insertBeforeMethodCallbackForMainWorld(v8::FunctionCallbackInfo<v8::Value> const&)
0xd3ddfee3(libmonochrome.so -api-arguments-inl.h:95 )v8::internal::Builtin_Impl_HandleApiCall(v8::internal::BuiltinArguments, v8::internal::Isolate*)
```

This stack trace ends with `blink::EventDispatcher::Dispatch()`
but references `vector.h:1047`.

There is not enough information in this trace to know what code inside `Dispatch()` triggered the crash.

## Getting the code

First of all, we need the correct source code.
This crash comes from Chrome version `69.0.3497.91`.
See of *Syncing and building a release tag* in [this doc](https://www.chromium.org/developers/how-tos/get-the-code/working-with-release-branches#TOC-Syncing-and-building-a-release-tag)
for how to check out code at a specific tag.

Now we can see `vector.h:1047` is actually the `CHECK_LT` in

```
T& at(size_t i) {
  CHECK_LT(i, size());
  return Base::Buffer()[i];
}
```

To get further, we need to look at the compiled code in the binary that produced the stack trace.

## Interpreting the addresses

The addresses that appear in the stack trace are memory addresses.
We need to translate them into offsets into the binary file.
A crash report should come with a memory map
that tells you the address at which every library and binary has been loaded.
So, subtracting this from the address in the trace
give the correct address for looking at the code.

In this example, the `libmonochrome.so` was loaded at `0xd24cd000`
so the code we are interested in is at `0x17c3a78`.

## Dumping the compiled code

[This doc](https://chromium.googlesource.com/chromium/src/+/main/docs/disassemble_code.md) describes how to dump the assembler code for a method from a binary.
In this example, it's a crash from an Android Chrome binary.
Only Googlers have access to the unstripped binary files needed for this example
but the steps are generic and work with any version of Chromium
(or indeed other binaries).

In this case, we can dump the entire `Dispatch()` method
and find `0x17c3a78`.
This looks like

```
 17c3a6c:       be00            bkpt    0x0000
 17c3a6e:       de05            udf     #5
 17c3a70:       be00            bkpt    0x0000
 17c3a72:       de05            udf     #5
 17c3a74:       be00            bkpt    0x0000
 17c3a76:       de05            udf     #5
 17c3a78:       be00            bkpt    0x0000
 17c3a7a:       de05            udf     #5
```


You don't need to be able to read ARM assembler to make some sense of this.
We're looking for a `CHECK_LT`
and we've found [`bkpt`](http://www.keil.com/support/man/docs/armasm/armasm_dom1361289865326.htm)
which makes sense.
It doesn't look like you can just execute your way to `0x17c3a78`,
so presumably we jump there.
Searching for that address elsewhere we find only one reference to it
as the target of a branch instruction.
It's the last line below:

```
/b/build/slave/official-arm/build/src/out/Release/../../third_party/blink/renderer/core/dom/events/event_dispatcher.cc:190
 17c38e0:       f010 0f30       tst.w   r0, #48 ; 0x30
 17c38e4:       f47f af47       bne.w   17c3776 <blink::EventDispatcher::Dispatch()+0xbe>
_ZNK5blink10MemberBaseINS_9EventPathELNS_28TracenessMemberConfigurationE0EEdeEv():
/b/build/slave/official-arm/build/src/out/Release/../../third_party/blink/renderer/platform/heap/member.h:91
 17c38e8:       6a48            ldr     r0, [r1, #36]   ; 0x24
 17c38ea:       f04f 0801       mov.w   r8, #1
 17c38ee:       f04f 0a00       mov.w   sl, #0
_ZNK3WTF6VectorIN5blink16NodeEventContextELj0ENS1_13HeapAllocatorEE4sizeEv():
/b/build/slave/official-arm/build/src/out/Release/../../third_party/blink/renderer/platform/wtf/vector.h:1035
 17c38f2:       6885            ldr     r5, [r0, #8]
 17c38f4:       e022            b.n     17c393c <blink::EventDispatcher::Dispatch()+0x284>
_ZNK5blink10MemberBaseINS_9EventPathELNS_28TracenessMemberConfigurationE0EEdeEv():
/b/build/slave/official-arm/build/src/out/Release/../../third_party/blink/renderer/platform/heap/member.h:91
 17c38f6:       6a48            ldr     r0, [r1, #36]   ; 0x24
_ZNK3WTF6VectorIN5blink16NodeEventContextELj0ENS1_13HeapAllocatorEE4sizeEv():
/b/build/slave/official-arm/build/src/out/Release/../../third_party/blink/renderer/platform/wtf/vector.h:1035
 17c38f8:       6882            ldr     r2, [r0, #8]
_ZN3WTF6VectorIN5blink16NodeEventContextELj0ENS1_13HeapAllocatorEE2atEj():
/b/build/slave/official-arm/build/src/out/Release/../../third_party/blink/renderer/platform/wtf/vector.h:1047
 17c38fa:       4590            cmp     r8, r2
 17c38fc:       f080 80bc       bcs.w   17c3a78 <blink::EventDispatcher::Dispatch()+0x3c0>
```

Before it you can see that we have code inlined from `vector.h:1047`
and looking further back up the code,
this appears to be a lot of inlined code from `vector.h` and `member.h`
all the way back up until you find `event_dispatcher.cc:190` which is

```
      if (DispatchEventAtTarget() == kContinueDispatching)
        DispatchEventAtBubbling();
```

This calls

```
inline EventDispatchContinuation EventDispatcher::DispatchEventAtTarget() {
  event_->SetEventPhase(Event::kAtTarget);
  event_->GetEventPath()[0].HandleLocalEvents(*event_);
  return event_->PropagationStopped() ? kDoneDispatching : kContinueDispatching;
}
```

which is marked as `inline`.
This explains why it's not even mentioned in the stack trace
(it's file and line info does not appear at all)
and why there is so much code between the `event_dispatcher.cc:190`
and the crash.

So the real stack trace is

```
vector.h:1047    WTF::Vector::at()
event_dispatcher.cc:241    blink::EventDispatcher::DispatchEventAtTarget
event_dispatcher.cc:190    blink::EventDispatcher::Dispatch()
```

It's possible that optimization can lead to more complex code
e.g. having multiple routes to the same piece of code.
In this case, things are pretty clear
and there are no jumps from further up the method
that land into the code we are looking at.
