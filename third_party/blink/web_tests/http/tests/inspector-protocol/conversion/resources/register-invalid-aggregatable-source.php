<?php
// The event-level header must be valid for the aggregatable one to be parsed at all.
header('Attribution-Reporting-Register-Source: {"source_event_id":"0","destination":"https://irrelevant.test"}');
header('Attribution-Reporting-Register-Aggregatable-Source: @');
?>
