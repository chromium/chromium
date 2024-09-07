# Security Guidelines for LLMs and other large models in Chrome

Large language models (LLMs), generative artificial intelligence (GenAI) models,
and other large machine learning (ML) models will find uses in Chromium and the
web. We will refer to all of these as _models_. This document outlines some
guidelines to help safely implement features using large models.

Our main security goals are to prevent arbitrary code execution, and prevent
user information disclosure between origins. It is not possible to prevent
people using Chrome from seeing model weights or predictions as this is not
feasible on the client devices where Chrome runs.

# Memory Safety

Models are, abstractly, layers of mathematical operations that mix inputs from
trustworthy and untrustworthy sources and produce output that will be used
elsewhere in Chrome. In practice these models are implemented in memory-unsafe
languages and may include convenience functions to parse complex data formats as
part of their pipelines. They should be treated the same way as other
memory-unsafe code implementing a feature in Chrome to comply with the
[rule-of-2](rule-of-2.md). Models processing untrustworthy complex data must be
sandboxed, and data should be provided using safe types.

## Complex formats

Models processing complex data -- such as images, audio or video -- could be
implemented using format helpers in their pipelines. To ensure memory safety any
parsing of complex formats should happen in a sandboxed, site-isolated process.
Either by sandboxing the model, or by parsing complex formats into accepted safe
formats before sending them to the process hosting the model.

### Exception - Tokenization

Where the only function of the model is to tokenize a string of text before
performing inference to produce an output this is not considered to be complex
processing.

## Untrustworthy input -> untrustworthy output

If an attacker can control any input to a model it must be assumed that they can
control all of its output. Models cannot be used to sanitize data, and their
output must be treated as untrustworthy content with an untrustworthy format.

Model output will either need to be parsed in a sandboxed process, or limited to
only outputting safe types (e.g. an array of floats).

## Mitigations

Models exposed to untrustworthy input can reduce the risk of exposing memory
safety flaws.

  * Use a tight sandbox
  * Provide model inputs over safe mojo types
  * Validate the size and format of input
  * Use a pipeline that only tokenizes then performs inference
  * Ensure input is in the same format as training data
  * Disable custom ops that might parse complex formatted data
  * Limit the size of the model output
  * Fuzz exposed APIs

# Side-Channels

Large models will necessarily be reused for several purposes. Where this happens
it is important that appropriate sessionization is used. It is likely that side
channels will exist that could leak some information about previous inputs.

# Model APIs

Models themselves are complex formats that represent complex graphs of
computation. APIs that allow web sites to specify and run models should be
designed so that these graphs and model inputs can be provided safely. Model
hosting should be managed by a trusted process to ensure only the right set of
operations can be reached by an untrustworthy model.

If a model's provenance can be verified (such as with Chrome's Component
Updater) then we can assume it is as safe as other Chrome code. This means that
where it runs is determined by what the model does, and the safety of the data
it consumes. Googlers should refer to internal guidelines for approved delivery
mechanisms in Chrome (go/tf-security-in-chrome,
go/chrome-genai-security-prompts).

# Other safety considerations

Models can output very convincing text. They may be used to summarize important
information (e.g. translating a legal form), or to produce writing for people
using Chrome (e.g. a letter to a bank). Models can produce incorrect output even
if they are not being deliberately steered to do so. People using Chrome should
have obvious indications that model output is being used, information about the
source of its inputs, and opportunity to review any text generated on their
behalf before it is submitted to a third party.

Models may output inappropriate material and where possible their output should
be filtered using reasonable safety filters and people should have mechanisms to
report and improve model outputs.

Model weights trained from on-device data may embody information about a person
using Chrome and should be treated like other sensitive data.
