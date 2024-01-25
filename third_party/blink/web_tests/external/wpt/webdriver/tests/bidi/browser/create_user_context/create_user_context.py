import pytest


@pytest.mark.asyncio
async def test_unique_id(bidi_session):
    first_context = await bidi_session.browser.create_user_context()
    assert isinstance(first_context, str)

    other_context = await bidi_session.browser.create_user_context()
    assert isinstance(other_context, str)

    assert first_context != other_context
