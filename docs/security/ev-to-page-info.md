# EV UI Moving to Page Info

As part of a series of data-driven
[changes](https://blog.chromium.org/2018/05/evolving-chromes-security-indicators.html)
to Chrome’s security indicators, the Chrome Security UX team is announcing a
change to the
[Extended Validation](https://en.wikipedia.org/wiki/Extended_Validation_Certificate)
certificate indicator on certain websites starting in Chrome 77. This doc
explains what’s being changed and why, as well as the supporting research
that guided this decision.

On HTTPS websites using [EV](https://en.wikipedia.org/wiki/Extended_Validation_Certificate)
certificates, Chrome 76 currently displays an EV badge to the left
of the URL bar that looks like this:

![Chrome 76 EV UI](ev-to-page-info-images/chrome-76-ev-bar.png "Chrome 76 EV
UI")

Starting in Version 77, Chrome will move this UI to Page Info, which is accessed
by clicking the lock icon:

![Chrome 77 Page Info UI](ev-to-page-info-images/chrome-77-page-info.png "Chrome
77 Page Info UI")

Through our own research as well as a survey of prior academic work, the Chrome
Security UX team has determined that the EV UI does not protect users as
intended (see [Further Reading](#Further-Reading) below). Users do not appear
to make secure choices (such as not entering password or credit card
information) when the UI is altered or removed, as would be necessary for EV UI
to provide meaningful protection. Further, the EV badge takes up valuable
screen real estate, can present
[actively confusing company names](https://www.typewritten.net/writer/ev-phishing/)
in prominent UI, and interferes with Chrome's product direction towards
neutral, rather than positive,
[display for secure connections](https://blog.chromium.org/2018/05/evolving-chromes-security-indicators.html).
Because of these problems and its limited utility, we believe it belongs better
in Page Info.

Altering the EV UI is a part of a wider trend among browsers to improve their
Security UI surfaces in light of recent advances in understanding of this
problem space. In 2018, Apple
[announced a similar change](https://cabforum.org/2018/06/06/minutes-for-ca-browser-forum-f2f-meeting-44-london-6-7-june-2018/#Apple-Root-Program-Update)
to Safari that coincided with the release of iOS 12 and macOS 10.14 and has
been implemented as such ever since.

### Information for embedders

This change is being incorporated into the Chrome-specific UI code and will not
affect embedders that are based solely on the underlying content layer.
Embedders that incorporate the Chrome-specific code will either take up these
changes or maintain a diff from the `main` Chromium branch.


## Further Reading

A series of academic research in the 2000s studied the EV UI in lab and survey
settings, and found that the EV UI was not protecting against phishing attacks
as intended. The Chrome Security UX team recently published a study that updated
these findings with a large-scale field experiment, as well as a series of
survey experiments.

No one single study conclusively determines that EV UI is completely ineffective
or cannot be made to be effective. However, we believe that the body of
research, as well as the product principles outlined above, together strongly
suggest that the EV UI does not belong in Chrome’s most visible UI surface.

### External Research:

*   [An evaluation of extended validation and picture-in-picture phishing attacks](https://www.adambarth.com/papers/2007/jackson-simon-tan-barth.pdf):
	surveys participants about IE 7’s EV UI and concludes that it did not help
    	users identify two types of phishing attacks, even after participants
    	received education about the UI.
*   [Exploring User Reactions to New Browser Cues for Extended Validation Certificates](http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.543.2117&rep=rep1&type=pdf):
	studies Firefox 3’s EV UI and found
	that users did not notice it. The researchers presented a re-designed
	indicator which some users did notice but did not use in their decision-
	making.
*   [Browser interfaces and extended validation SSL certificates: An empirical study](http://people.scs.carleton.ca/~paulv/papers/ccsw09.pdf):
	explores a new EV UI design in comparison to IE 7’s design. The researchers
	showed promising results on some axes but did not study whether the new
	design actually helps users detect attacks.
*   [The Emperor’s New Security Indicators: An evaluation of website authentication and the effect of role playing on usability studies](http://andyozment.com/papers/emperor.pdf):
	does not study EV specifically, but studies other positive (non-warning)
	security indicators for website authentication via lab study and finds that
	users do not notice their absence.

### Chrome Research:

*   [The Web’s Identity Crisis: Understanding the Effectiveness of Website Identity Indicators](https://storage.googleapis.com/pub-tools-public-publication-data/pdf/400599205ab5a1c9efa03e2a7c127eb8200bf288.pdf):
	a large-scale field experiment in which the EV UI was removed for a random
	subset of users, and a wide variety of user behavior metrics did not change,
	suggesting that the EV UI is not having its intended effect. Survey
	experiments also confirm that users do not react as intended to positive or
	neutral security UI.
*   [Rethinking Connection Security Indicators](https://storage.googleapis.com/pub-tools-public-publication-data/pdf/45366.pdf):
	does not study EV specifically, but studies users’ reaction to other
	connection security indicators like the lock icon via survey, and finds that
	users are widely confused about their meaning. Informs Chrome’s overall
	direction to remove positive security indicators.
