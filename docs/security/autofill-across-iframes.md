# Autofill across iframes

Chrome Autofill fills in frame-transcending forms like the following example.

```html
<!-- Top-level document URL: https://merchant.example/... -->
<form>
  Cardholder name:
  <input id=name>

  Credit card number:
  <iframe src="https://psp.example/..." allow=autofill>
    #document
      <input id=num>
  </iframe>

  Expiration date:
  <input id=exp>

  Verification code:
  <iframe src="https://psp.example/..." allow=autofill>
    #document
      <input id=cvc>
  </iframe>

  <iframe src="https://ads.example/...">
    #document
      <input id=account>
  </iframe>
</form>
```

## The security policy

An autofill fills a form control *candidate* only if one of the following is
true:

-   the autofill's origin and the *candidate*'s origin are the [same origin];
-   the candidate's origin and the top-level origin are the [same origin].

The second bullet point is going to be replaced with the new policy-controlled
feature [`autofill`], which allows a page to select which embedded content is
(not) trusted to receive autofill input. Then, an autofill will fill a form
control *candidate* only if one of the following is true:

-   the autofill's origin and the *candidate*'s origin are the [same origin];
-   the policy-controlled feature [`autofill`] is enabled in the *candidate*'s
    [node document].

Starting in Chrome 153 (October 2026), autofill suggestions will include a
[warning] if they'd fill a field in whose [node document] [`autofill`] is
disabled.

In the long term, the first bullet point (and the warnings) will be removed.

Note that the above conditions are necessary but not sufficient for Chrome to
autofill a field. In particular, Chrome does not fill credentials across frames.
Chrome also avoids autofilling sensitive data, such as credit card numbers,
across origins.

The terminology used above is defined in the [appendix](#appendix-terminology).

The policy is implemented in [FormForest::GetRendererFormsOfBrowserForm()].

## The rationale

The example form above exhibits a common pattern: at the time of writing, about
20% of the payment forms on the web span multiple origins. Most commonly, the
cardholder name field's origin is the top-level origin, whereas the credit card
number is in a cross-origin iframe hosted by the payment service provider (PSP).

These iframes are typically styled so that they seamlessly integrate with the
merchant's page -- the user is not made aware that multiple frames and origins
are involved. Yet the different origins isolate the payment information from the
merchant's website, which helps them comply with the payment card industry's
data security standard (see Section 2.2.3 of the [PCI-DSS best practices]).

Chrome Autofill's objective is to fill fields that the user expects to be
filled, even if those fields cross origins, while protecting the user against
possibly malicious sub-frames.

The following table illustrates which fields may be filled in our example form
depending on the autofill's origin:

| Autofill's origin          | `name`   | `num`    | `exp`    | `cvc`    | `account` |
|----------------------------|:--------:|:--------:|:--------:|:--------:|:---------:|
| `https://merchant.example` | &#10004; | &#10004; | &#10004; | &#10004; | &#10006;  |
| `https://psp.example`      | &#10004; | &#10004; | &#10004; | &#10004; | &#10006;  |
| `https://ads.example`      | &#10004; | &#10006; | &#10004; | &#10006; | &#10004;  |

## Appendix: Terminology

An *autofill* is an operation that fills one or many form control elements in
the [fully active descendants of a top-level traversable with user attention].
An autofill can only be initiated on a [focused] form control element.

An autofill *fills a form control* if it changes the form control's [value]. The
value after the autofill is the *form control's autofill value*.

A *form control's origin* is its [node document]'s [origin].
An *autofill's origin* is the [focused] form control's origin.
The *top-level origin* is the [top-level traversable]'s [active document]'s [origin].

The policy-controlled feature *[`autofill`] is enabled in a document* if the
[Is feature enabled in document for origin?] algorithm on [`autofill`], the
document, and the document's [origin] returns `Enabled`.

[FormForest::GetRendererFormsOfBrowserForm()]: https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/browser/form_forest.cc;l=618-623;drc=94fbbc584c5d42f0097a9cb28b355853d2b34658
[active document]: https://html.spec.whatwg.org/#nav-document
[Is feature enabled in document for origin?]: https://w3c.github.io/webappsec-permissions-policy/#algo-is-feature-enabled
[focused]: https://html.spec.whatwg.org/#focused
[fully active descendants of a top-level traversable with user attention]: https://html.spec.whatwg.org/#fully-active-descendant-of-a-top-level-traversable-with-user-attention
[same origin]: https://html.spec.whatwg.org/multipage/browsers.html#same-origin
[node document]: https://dom.spec.whatwg.org/#concept-node-document
[origin]: https://dom.spec.whatwg.org/#concept-document-origin
[PCI-DSS best practices]: https://www.pcisecuritystandards.org/
[`autofill`]: https://github.com/explainers-by-googlers/safe-text-input/blob/main/autofill.md
[top-level traversable]: https://html.spec.whatwg.org/#top-level-traversable
[value]: https://html.spec.whatwg.org/#concept-fe-value
[warning]: https://github.com/explainers-by-googlers/safe-text-input/raw/main/images/warning-autofill.png
