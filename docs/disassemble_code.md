# Dumping the compiled code from a chrome binary

[Rendered](https://chromium.googlesource.com/chromium/src/+/master/docs/disassemble_code.md)

## Background

Sometimes you want to look at the disassembled code of a method,
e.g. you have a stack trace like

```
0xd3c90a78(libmonochrome.so -vector.h:1047 ) blink::EventDispatcher::Dispatch()
0xde86db5a
0xd3c8fcdd(libmonochrome.so -event_dispatcher.cc:59 )blink::EventDispatcher::DispatchEvent(blink::Node&, blink::Event*)
```

Where something from `vector.h` has been inlined in `Dispatch()`
and so you don't know what line in of `Dispatch()` is actually crashing.
Or you want to compare the code before and after a "no-op" change
that has resulted in a perf regression
or you want to see what methods have been inlined in the final binary
(e.g. AFDO can result in more/less inlining,
depending what was observed when the AFDO data was collected).

This doc tells you how to dump the assembly for a method
and how to deal with stack traces involving inlined code.

## Differences for Googlers

There are some tasks,
like disassembling official release builds of chrome
that have Google-specific instructions.
You can find them [here](https://goto.google.com/disassemble-chrome-code)

## Building an unstripped binary

Below are the ninja arguments for the `rel_ng` trybot
but with extra symbols enabled.
We can include even more symbols with `symbol_level` of `2` instead of `1`
but the binary size is much greater.

```
is_official_build = true
dcheck_always_on = true
is_component_build = false
is_debug = false
blink_symbol_level = 1
symbol_level = 1

# Restricted options. May not make sense for non-Googlers.
use_goma = true
proprietary_codecs = true
ffmpeg_branding = "Chrome"
```

This should be close to a release-optimized binary
e.g. it applies Automatic Feedback Directed Optimization.
You might want to switch to `dcheck_always_on=false`.

To build, run

```shell
ninja -C out/RelNgSym
```

You probably will want to add some more command line options to this,
depending on your usual build paralellism.

## Getting the right objdump

You can dump the asm from the binary or `.so` file using `objdump`.
You can dump the symbols using `nm`.
Both are from GNU binutils.
For x86 code, if you are running Linux,
you probably have it already.

If you are trying to examine Android code,
it's likely ARM or MIPS.
In that case you can install an alternate binutils package
or (maybe better) use the one that ships in Android's SDK.

## Finding the address of the compiled code

So, let's say we are looking for `blink::StyleInvalidator::Invalidate`.
First we dump out the symbols from the binary.
This takes a few seconds.

```shell
nm out/RelNgSym/chrome > /tmp/symbols
```

Now we need to get the start and end address of the function.
This is a little hacky but we can find the end address
by finding the next function
(with symbol level `2` we get entries
which give an address and length
but nm is slower).
So now run

```shell
grep -A1 "blink::StyleInvalidator::Invalidate(" /tmp/symbols
```

and you should see the something like

```
000000000769d230 t blink::StyleInvalidator::Invalidate(blink::Element&, blink::StyleInvalidator::SiblingData&)
000000000769ce10 t blink::StyleInvalidator::Invalidate(blink::Document&, blink::Element*)
000000000769d950 t blink::StyleInvalidator::SiblingData::MatchCurrentInvalidationSets(blink::Element&, blink::StyleInvalidator&)
```

So we have found 2 overloads for this symbol
and let's say we want the latter.
So the function starts at `0x769ce10`
and ends at `0x769d950`
(the address of the next symbol).

## Dumping code

Using the relevant objdump binary, we can now dump the assembly code with

```
objdump -j .text -D -l -C --start-addr=0x769ce10 --stop-addr=0x769d950 out/RelNgSym/chrome
```

and you get something like

```
out/RelNgSym/chrome:     file format elf64-x86-64


Disassembly of section .text:

000000000769ce10 <blink::StyleInvalidator::Invalidate(blink::Document&, blink::Element*)>:
_ZN5blink16StyleInvalidator10InvalidateERNS_8DocumentEPNS_7ElementE():
./../../third_party/blink/renderer/core/css/invalidation/style_invalidator.cc:31
 769ce10:	55                   	push   %rbp
 769ce11:	48 89 e5             	mov    %rsp,%rbp
 769ce14:	41 57                	push   %r15
 769ce16:	41 56                	push   %r14
 769ce18:	41 55                	push   %r13
 769ce1a:	41 54                	push   %r12
 769ce1c:	53                   	push   %rbx
 769ce1d:	48 81 ec 28 01 00 00 	sub    $0x128,%rsp
 769ce24:	49 89 d4             	mov    %rdx,%r12
 769ce27:	49 89 f7             	mov    %rsi,%r15
 769ce2a:	49 89 fe             	mov    %rdi,%r14
 769ce2d:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
 769ce34:	00 00
 769ce36:	48 89 45 d0          	mov    %rax,-0x30(%rbp)
InlineBuffer():
./../../third_party/blink/renderer/platform/wtf/vector.h:864
 769ce3a:	4c 8d ad c8 fe ff ff 	lea    -0x138(%rbp),%r13
VectorBufferBase():
./../../third_party/blink/renderer/platform/wtf/vector.h:465
 769ce41:	4c 89 ad b8 fe ff ff 	mov    %r13,-0x148(%rbp)
 769ce48:	48 c7 85 c0 fe ff ff 	movq   $0x10,-0x140(%rbp)
 769ce4f:	10 00 00 00
SiblingData():
./../../third_party/blink/renderer/core/css/invalidation/style_invalidator.h:87
 769ce53:	c7 45 c8 00 00 00 00 	movl   $0x0,-0x38(%rbp)
GetFlag():
./../../third_party/blink/renderer/core/dom/node.h:909
 769ce5a:	8b 46 10             	mov    0x10(%rsi),%eax
 769ce5d:	66 85 c0             	test   %ax,%ax
_ZN5blink16StyleInvalidator10InvalidateERNS_8DocumentEPNS_7ElementE():
./../../third_party/blink/renderer/core/css/invalidation/style_invalidator.cc:34
 769ce60:	0f 88 28 01 00 00    	js     769cf8e <blink::StyleInvalidator::Invalidate(blink::Document&, blink::Element*)+0x17e>
./../../third_party/blink/renderer/core/css/invalidation/style_invalidator.cc:41
 769ce66:	4d 85 e4             	test   %r12,%r12
 769ce69:	0f 84 85 00 00 00    	je     769cef4 <blink::StyleInvalidator::Invalidate(blink::Document&, blink::Element*)+0xe4>
 769ce6f:	48 8d 95 b8 fe ff ff 	lea    -0x148(%rbp),%rdx
./../../third_party/blink/renderer/core/css/invalidation/style_invalidator.cc:42
 769ce76:	4c 89 f7             	mov    %r14,%rdi
 769ce79:	4c 89 e6             	mov    %r12,%rsi
 769ce7c:	e8 af 03 00 00       	callq  769d230 <blink::StyleInvalidator::Invalidate(blink::Element&, blink::StyleInvalidator::SiblingData&)>
IsEmpty():
./../../third_party/blink/renderer/platform/wtf/vector.h:1039
 769ce81:	83 bd c4 fe ff ff 00 	cmpl   $0x0,-0x13c(%rbp)
_ZN5blink16StyleInvalidator10InvalidateERNS_8DocumentEPNS_7ElementE():
./../../third_party/blink/renderer/core/css/invalidation/style_invalidator.cc:43
 769ce88:	74 56                	je     769cee0 <blink::StyleInvalidator::Invalidate(blink::Document&, blink::Element*)+0xd0>
 769ce8a:	4c 89 e3             	mov    %r12,%rbx
 769ce8d:	0f 1f 00             	nopl   (%rax)
 769ce90:	48 8b 5b 30          	mov    0x30(%rbx),%rbx
NextSibling():
./../../third_party/blink/renderer/core/dom/element_traversal.h:530 (discriminator 2)
 769ce94:	48 85 db             	test   %rbx,%rbx
 769ce97:	74 47                	je     769cee0 <blink::StyleInvalidator::Invalidate(blink::Document&, blink::Element*)+0xd0>

...
```


## Further reading

https://chromium.googlesource.com/chromium/src/+/master/docs/linux_minidump_to_core.md#Source-debugging

https://www.chromium.org/developers/how-tos/debugging-on-windows
