# Tokens

## Overview

This directory contains strongly-typed wrappers (using
[`base::TokenType<...>`](/base/types/token_type.h)) of
[`base::UnguessableToken`](/base/unguessable_token.h)
for tokens that are commonly passed between browsers and renderers. The strong
typing is to prevent type confusion as these tokens are passed around. To support
strong typing through the entire stack (including IPC) these tokens additionally
include `content/` and `blink/` specific typemaps, as well as Mojo struct definitions.

## Adding a new token

Suppose you want to add a new token type. You would do the following:

 - Add a new C++ token type to
   [`/third_party/blink/public/common/tokens/tokens.h`](/third_party/blink/public/common/tokens/tokens.h).
 - Add an equivalent Mojom token type to
   [`/third_party/blink/public/mojom/tokens/tokens.mojom`](/third_party/blink/public/mojom/tokens/tokens.mojom).
   Be sure to follow the convention that the struct contains a single
   `base.mojom.UnguessableToken` member named `value`.
 - Create a new Mojom traits declaration to
   [`/third_party/blink/public/common/tokens/tokens_mojom_traits.h`](/third_party/blink/public/common/tokens/tokens_mojom_traits.h).
   Use the templated [`TokenMojomTraitsHelper<...>`](/third_party/blink/public/common/token_mojom_traits_helper.h) helper class.
 - Update [`mojom/tokens/BUILD.gn`](third_party/blink/public/mojom/tokens/BUILD.gn) and add a new
   typemap definition for the token to the `shared_cpp_typemaps` section.
 - If your token needs to be sent via legacy IPC as well, add the appropriate
   definition to [`/content/common/content_param_traits.h`](/content/common/content_param_traits.h).
