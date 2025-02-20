import pytest

from . import set_simulate_preconnected_peripheral

pytestmark = pytest.mark.asyncio


async def test_simulate_preconnected_peripheral(bidi_session, top_context,
                                                test_page):
    try:
        await set_simulate_preconnected_peripheral(
            bidi_session,
            top_context,
            test_page,
            "09:09:09:09:09:09",
            "LE device",
            [{"key": 17, "data": "AP8BAX8="}],
            ["12345678-1234-5678-9abc-def123456789"],
        )
    except Exception as e:
        pytest.fail(
            f"set_simulate_preconnected_peripheral raised an exception: {e}")
