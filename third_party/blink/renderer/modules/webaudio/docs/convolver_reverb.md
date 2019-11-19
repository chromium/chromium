# Convolution Reverb

A [convolution reverb](http://en.wikipedia.org/wiki/Convolution_reverb) can be
used to simulate an acoustic space with very high quality. It can also be used
as the basis for creating a vast number of unique and interesting special
effects.  This technique is widely used in modern professional audio and motion
picture production, and is an excellent choice to create room effects in a game
engine.

Creating a well-optimized real-time convolution engine is one of the more
challenging parts of the Web Audio API implementation.  When convolving an input
audio stream of unknown (or theoretically infinite) length, the
[overlap-add](http://en.wikipedia.org/wiki/Overlap-add_method) approach is used,
chopping the input stream into pieces of length L, performing the convolution on
each piece, then re-constructing the output signal by delaying each result and
summing.


## Overlap-Add Convolution

![Depiction of overlap-add
algorithm](http://upload.wikimedia.org/wikipedia/commons/7/77/Depiction_of_overlap-add_algorithm.png)

Direct convolution is far too computationally expensive due to the extremely
long impulse responses typically used.  Therefore an approach using
[FFTs](http://en.wikipedia.org/wiki/FFT) must be used.  But naively doing a
standard overlap-add FFT convolution using an FFT of size N with L=N/2, where N
is chosen to be at least twice the length of the convolution kernel
(zero-padding the kernel) to perform each convolution operation in the diagram
above would incur a substantial input to output pipeline latency on the order of
L samples.  Because of the enormous audible delay, this simple method cannot be
used.  Aside from the enormous delay, the size N of the FFT could be extremely
large.  For example, with an impulse response of 10 seconds at 44.1Khz, N would
equal 1048576 (2^20).  This would take a very long time to evaluate.
Furthermore, such large FFTs are not practical due to substantial phase errors.

## Optimizations and Tricks

There exist several clever tricks which break the impulse response into smaller
pieces, performing separate convolutions, then combining the results (exploiting
the property of linearity).  The best ones use a divide and conquer approach
using different size FFTs and a direct convolution for the initial (leading)
portion of the impulse response to achieve a zero-latency output.  There are
additional optimizations which can be done exploiting the fact that the tail of
the reverb typically contains very little or no high-frequency energy.  For this
part, the convolution may be done at a lower sample-rate...

Performance can be quite good, easily done in real-time without creating undo
stress on modern mid-range CPUs.  A multi-threaded implementation is really
required if low (or zero) latency is required because of the way the buffering /
processing chunking works.  Achieving good performance requires a highly
optimized FFT algorithm.

### Multi-channel convolution

It should be noted that a convolution reverb typically involves two convolution
operations, with separate impulse responses for the left and right channels in
the stereo case.  For 5.1 surround, at least five separate convolution
operations are necessary to generate output for each of the five channels.
However, we only need to support stereo, as given in [Channel Configurations for
Input, Impulse Response and
Output](https://webaudio.github.io/web-audio-api/#Convolution-channel-configurations).

# Convolution Engine Implementation

## FFTConvolver (short convolutions)

The
[`FFTConvolver`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/audio/fft_convolver.h)
is able to do short convolutions with the FFT size N being at least twice as
large as the length of the short impulse response.  It incurs a latency of N/2
sample-frames.  Because of this latency and performance considerations, it is
not suitable for long convolutions.  Multiple instances of this building block
can be used to perform extremely long convolutions.

![description of FFT convolver](images/fft-convolver.png)

## ReverbConvolver (long convolutions)

The
[`ReverbConvolver`](https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/audio/reverb_convolver.h)
is able to perform extremely long real-time convolutions on a single audio
channel.  It uses multiple `FFTConvolver` objects as well as an input buffer and
an accumulation buffer.  Note that it's possible to get a multi-threaded
implementation by exploiting the parallelism.  Also note that the leading
sections of the long impulse response are processed in the real-time thread for
minimum latency.  In theory it's possible to get zero latency if the very first
FFTConvolver is replaced with a DirectConvolver (not using a FFT).

![description of reverb convolver](images/reverb-convolver.png)
