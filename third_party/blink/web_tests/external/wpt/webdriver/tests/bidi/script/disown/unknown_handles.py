import pytest

from webdriver.bidi.modules.script import ContextTarget, RealmTarget

pytestmark = pytest.mark.asyncio


# script.disown ignores handles that don't exist in the target realm, so these
# calls succeed without any value having been created with "root" ownership.


@pytest.mark.parametrize("handles", [[], ["foo"], ["foo", "bar"]])
async def test_context(bidi_session, top_context, handles):
    await bidi_session.script.disown(
        handles=handles,
        target=ContextTarget(top_context["context"]))


@pytest.mark.parametrize("handles", [[], ["foo"], ["foo", "bar"]])
async def test_sandbox(bidi_session, top_context, handles):
    await bidi_session.script.disown(
        handles=handles,
        target=ContextTarget(top_context["context"], "sandbox"))


@pytest.mark.parametrize("handles", [[], ["foo"], ["foo", "bar"]])
async def test_realm(bidi_session, top_context, handles):
    result = await bidi_session.script.evaluate(
        raw_result=True,
        expression="1",
        await_promise=False,
        target=ContextTarget(top_context["context"]),
    )

    await bidi_session.script.disown(
        handles=handles,
        target=RealmTarget(result["realm"]))
