# Decision API Backends

This directory contains dedicated backends for the On-Device Model Decision API. Currently, Laya is the only backend implemented here.

## Laya Overview

Laya is a small bidirectional transformer encoder (ModernBERT backbone) paired with a typed decision head designed for fast structured decision-making (multiple-choice classification, rubric scoring, and boolean evaluation).

## Laya Model Format

The runtime `.tflite` model is repackaged from HuggingFace's [`convaiinnovations/laya`](https://huggingface.co/convaiinnovations/laya) with the following modifications:

- **Quantization**: Weights are quantized to `int8`.
- **Entrypoint signatures**: Exported to TFLite with multiple `seq_N` entrypoint signatures for sequence lengths `N` from 8 to 8192 in powers of 2, allowing `LayaModel` to select the smallest signature that fits the tokenized query and input.
- **Embedded metadata buffers**:
  - `"tokenizer"`: The HuggingFace tokenizer converted into a serialized SentencePiece `ModelProto`.
  - `"temperature"`: Per-question-type calibration temperatures (`float32[3]` for choice, score, and boolean).

## Laya Query Format

`LayaQueryAdapter` adapts a `mojom::DecisionQuery` and runtime `input` text into the tensor inputs expected by the Laya model. Each question in the query corresponds to one batch element `b` (`0 <= b < questions().size()`).

### Model Signature Inputs

Inputs for each `seq_N` signature in the model (where `max_options` is the fixed option capacity shared by all `seq_N` signatures in the model, not the maximum across query questions):
- `input_ids` (`int32[batch_size, seq_len]`): Token ID sequence for each question, constructed by `ProcessInput(tokenizer, input)` and right-padded with `[PAD]` to the selected signature's `seq_len`.
- `attention_mask` (`int32[batch_size, seq_len]`): `1` for active tokens from `ProcessInput(tokenizer, input)` and `0` for trailing `[PAD]` tokens.
- `marker_pos` (`int32[batch_size, max_options]`): 0-based token index in `input_ids[b]` of the `[MASK]` token preceding each candidate option `i` (`0` for unused option slots where `i >= num_options`).
- `marker_mask` (`int32[batch_size, max_options]`): `1` for valid option slots (`i < num_options`) and `0` for unused option slots.
- `qtype` (`int32[batch_size]`): Question type ID per question (`0` = choice, `1` = score, `2` = boolean).

### Token-Level Format

Token-level format of `input_ids[b]` (before right-padding with `[PAD]` to `seq_len`):

```
[CLS] <head> [SEP] [MASK] <opt_0> ... [MASK] <opt_k> [SEP] <input> [SEP]
|______________________ prefix_ids ______________________|
```

- **Special tokens**: Resolved from the SentencePiece vocabulary as `[CLS]` (`"[CLS]"` or `"<bos>"`), `[SEP]` (`"[SEP]"` or `"<eos>"`), `[PAD]` (`"[PAD]"` or `"<pad>"`), and `[MASK]` (`"[MASK]"` or `"<mask>"`). Literal occurrences of these special token strings in query or input text are replaced with `" "` before tokenization, and `Tokenizer::Tokenize()` strips any `[CLS]`, `[SEP]`, `[PAD]`, or `[MASK]` token IDs post-`Encode()` so normalized or `USER_DEFINED` special tokens in untrusted text cannot spoof framing or option markers.
  - The English model's tokenizer uses the BERT-style special tokens (`[CLS]`, etc...), while the multilingual tokenizer uses angle bracket special tokens (`<bos>`, etc...).
- **`<head>`**: Tokenized from `"<question_type_name> question: <instruction>"`, where `<question_type_name>` is `"choice"`, `"score"`, or `"noul"` (for `kChoice`, `kScore`, and `kBoolean` respectively), and `<instruction>` is `question.prompt`.
- **`[MASK] <opt_i>`**: Repeated for each candidate option `0 <= i < num_options` (up to `max_options`). Consists of a single `[MASK]` token (whose index in `input_ids[b]` is recorded in `marker_pos`) followed by the tokenized option text (prefixed with a leading space and truncated to at most 48 tokens):
  - `kChoice` / `kBoolean`: `" <label>"` or `" <label>: <description>"` when `description` is non-empty.
  - `kScore`: `" level <i>: <criteria>"`, where `<criteria>` is `description` if non-empty, otherwise `label`.
- **Prefix budgeting**: After reserving 4 tokens for `[CLS]`/`[SEP]` framing and at least 4 tokens for `<input>`, `<head>` and all `[MASK] <opt_i>` segments are budgeted to fit within the remaining `max_seq_len - 8` tokens. If the options leave fewer than 16 tokens for `<head>`, each option is uniformly truncated (to at least 4 tokens including `[MASK]`), and `<head>` is truncated to the remaining budget (requiring at least 8 tokens unless `<head>` is already shorter). `LayaQueryAdapter::Create()` fails if all candidate options cannot fit within this budget.
- **`<input>`**: Tokenized from the runtime `input` string passed to `ProcessInput(tokenizer, input)`, truncated to fit the remaining sequence capacity (`max_seq_len - prefix_ids.size() - 1`), and followed by a closing `[SEP]` token. (TODO(crbug.com/565849508): Cap raw `input` size and report token overflow as `QuotaExceededError` instead of silently truncating.)
