# Autofill across iframes

Chrome Autofill fills in frame-transcending forms like the following pseudo-code
example.

```
<!-- Top-level document URL: https://merchant.example/... -->
<form>
  Cardholder name:    <input id="name">
  Credit card number: <iframe src="https://psp.example/..." allow="shared-autofill"><input id="num"></iframe>
  Expiration date:    <input id="exp">
  CVC:                <iframe src="https://psp.example/..." allow="shared-autofill"><input id="cvc"></iframe>
                      <iframe src="https://ads.example/..."><input id="account"></iframe>
</form>
```

This applies to address and payment information, but not to passwords.

## The security policy

An autofill fills a form control *candidate* only if one of the following is true:

-   the autofill's origin and the *candidate*'s origin are the [same origin];
-   [shared-autofill] is enabled in the *candidate*'s [node document] and one of
    the following is true:
    -   the autofill's origin and the top-level origin are the [same origin];
    -   the candidate's origin and the top-level origin are the [same origin] and the
        *candidate*'s autofill value is non-sensitive.

The terminology used above is defined in the [appendix](#appendix-terminology).

This policy is the [eligibility for autofill] definition plus the additional
"... and one of the following is true" conjunct in the [shared-autofill] clause.

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
possibly malicious sub-frames. Intuitively, we support two "directions":

-   "Downwards": An autofill may fill fields in descendant documents where
    [shared-autofill] is enabled. In our example, an autofill initiated on the
    cardholder name field may fill the credit card number field.
-   "Upwards": An autofill may fill certain values in ancestor documents. In our
    example, an autofill initiated on the credit card number field may fill the
    cardholder name field.

We restrict the values that may be filled "upwards" especially to prevent
leaking sensitive payment information -- credit card numbers and CVCs that the
PCI-DSS intends to protect -- into the merchant's page. The "non-sensitive"
values that we allow to be filled "upwards" are credit card types, cardholder
names, and expiration dates.

The terms "upwards" and "downwards" are imprecise: our security policy doesn't
refer to the [top-level traversable]'s [node document], but rather to its
[origin], the top-level origin. This way, Autofill works the same when, for example,
the cardholder name is hosted in a same-origin iframe: `<iframe
src="https://merchant.example/..."><input id="name"></iframe>`.

Our security policy does not allow "upwards" or "downwards" filling to and from
arbitrary documents. It only allows filling "upwards to" main-origin documents
and "downwards from" main-origin documents. This simplifies reasoning about the
security policy as well as the implementation, and is still sufficient for
real-world payment forms.

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

A form control's autofill value is *non-sensitive* if it is a credit card type,
a cardholder name, or a credit card expiration date.

A *form control's origin* is its [node document]'s [origin].
An *autofill's origin* is the [focused] form control's origin.
The *top-level origin* is the [top-level traversable]'s [active document]'s [origin].

*[shared-autofill] is enabled in a document* if the [Is feature enabled in
document for origin?] algorithm on [shared-autofill], the document, and the
document's [origin] returns `Enabled`.

*TODO*: Update link to [eligibility for autofill] once the
[PR](https://github.com/whatwg/html/pull/8801) is closed.

[FormForest::GetRendererFormsOfBrowserForm()]: https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/browser/form_forest.cc;l=618-623;drc=94fbbc584c5d42f0097a9cb28b355853d2b34658
[active document]: https://html.spec.whatwg.org/#nav-document
[eligibility for autofill]: https://schwering.github.io/html/#eligible-for-autofill
[Is feature enabled in document for origin?]: https://w3c.github.io/webappsec-permissions-policy/#algo-is-feature-enabled
[focused]: https://html.spec.whatwg.org/#focused
[fully active descendants of a top-level traversable with user attention]: https://html.spec.whatwg.org/#fully-active-descendant-of-a-top-level-traversable-with-user-attention
[same origin]: https://html.spec.whatwg.org/multipage/browsers.html#same-origin
[node document]: https://dom.spec.whatwg.org/#concept-node-document
[origin]: https://dom.spec.whatwg.org/#concept-document-origin
[PCI-DSS best practices]: https://www.pcisecuritystandards.org/
[shared-autofill]: https://schwering.github.io/shared-autofill/
[top-level traversable]: https://html.spec.whatwg.org/#top-level-traversable
[value]: https://html.spec.whatwg.org/#concept-fe-value
